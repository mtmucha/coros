#ifndef COROS_INCLUDE_DEQUE_H_
#define COROS_INCLUDE_DEQUE_H_

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <memory>
#include <optional>
#include <cstddef>
#include <vector>

#include "task_life_time.h"

namespace coros {
namespace detail {

#ifdef __cpp_lib_hardware_interference_size
  using std::hardware_constructive_interference_size;
  using std::hardware_destructive_interference_size;
#else
  constexpr std::size_t hardware_constructive_interference_size = sizeof(uint_fast64_t);
  constexpr std::size_t hardware_destructive_interference_size = sizeof(uint_fast64_t);
#endif

inline constexpr size_t kDefaultBufferSize = 512;

template <typename T>
class CircularBuffer {
 public:
  CircularBuffer(uint_fast64_t init_size)
      : size_(init_size), modulo_(size_ - 1), array_(std::make_unique<T[]>(size_)){};

  CircularBuffer(const CircularBuffer&) = delete;
  CircularBuffer(CircularBuffer&&) = delete;

  CircularBuffer& operator=(const CircularBuffer&) = delete;
  CircularBuffer& operator=(CircularBuffer&&) = delete;

  // Keeping the size of the buffer a power of 2 
  // allows us to use modulo
  void put(uint_fast64_t pos, T&& elem) noexcept {
    array_[pos & modulo_] = elem;
  }

  T get(uint_fast64_t pos) noexcept {
    return array_[pos & modulo_];
  }

  CircularBuffer<T>* grow(uint_fast64_t bottom, uint_fast64_t top) {
    CircularBuffer<T>* new_buffer = new CircularBuffer{4 * size_};
    for (uint_fast64_t i = top; i < bottom; i++) {
      new_buffer->put(i, get(i));
    }
    return new_buffer;
  }

  uint_fast64_t size() { return size_; }

  T& operator[](uint_fast64_t pos) { return array_[pos]; }

 private:
  uint_fast64_t size_;
  uint_fast64_t modulo_;
  std::unique_ptr<T[]> array_;
};


class Dequeue {
 public:
  Dequeue()
      : top_(1),
        bottom_(1),
        buffer_(new CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>(kDefaultBufferSize)), 
        old_buffers_(64) {};

  explicit Dequeue(uint_fast64_t init_buffer_size)
      : top_(1),
        bottom_(1), buffer_(new CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>(init_buffer_size)), 
        old_buffers_(64) {};

  explicit Dequeue(uint_fast64_t init_buffer_size, uint_fast64_t old_buffers_size)
      : top_(1),
        bottom_(1), buffer_(new CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>(init_buffer_size)), 
        old_buffers_(old_buffers_size) {};
  
  // Deletes coroutine states, which lifetimes are manged by ThreadPool.
  ~Dequeue() {
    // Calling delete for each old buffer, which release the memory
    // allocated in unique_ptr.
    for (auto& buffer : old_buffers_) {
      delete buffer;
    }

    // Destroy tasks in the current buffer. The current buffer
    // has all the coroutine handles. That were not processed.
    auto buffer = buffer_.load();
    for(uint_fast64_t i = top_; i < bottom_;i++) {
      auto task = buffer->get(i);
      // If there are any tasks that are managed by threadpool, they should
      // be destroyed. Otherwise the tasks are destroyed when they go
      // out of scope.
      if (task.second == TaskLifeTime::THREAD_POOL_MANAGED) 
        task.first.destroy();
    }

    // Deletes the buffer, freed by the unique_ptr that holds it in the 
    // CircularBuffer.
    delete buffer_;
  }

  // Overload for R-values.
  void pushBottom(std::pair<std::coroutine_handle<>, TaskLifeTime>&& handle) {
    uint_fast64_t bottom = bottom_.load(std::memory_order_relaxed);
    uint_fast64_t top = top_.load(std::memory_order_acquire);
    CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>* cb =
        buffer_.load(std::memory_order_relaxed);

    // Check if queue is full 
    if (bottom - top > cb->size() - 1) {
      old_buffers_.push_back(cb);
      cb = cb->grow(bottom, top);
      buffer_.store(cb, std::memory_order_relaxed);
    }
    
    cb->put(bottom, std::move(handle));
    // std::atomic_thread_fence(std::memory_order_release);
    // __tsan_release(&bottom_);
    // bottom_.store(bottom + 1, std::memory_order_relaxed);
    bottom_.store(bottom + 1, std::memory_order::release);
  };

  std::optional<std::coroutine_handle<>> steal() {
    std::uint_fast64_t top = top_.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::uint_fast64_t bottom = bottom_.load(std::memory_order_acquire);

    if (top < bottom) {
      std::pair<std::coroutine_handle<>, TaskLifeTime> return_handle = buffer_.load(std::memory_order_acquire)->get(top);

      if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                        std::memory_order_relaxed)) {
        // failsed to steal the item
        return {};
      }
      // return stolen item
      return return_handle.first;
    }
    // return empty value queue is empty
    return {};
  };

  std::optional<std::coroutine_handle<>> popBottom() {
    uint_fast64_t bottom = bottom_.load(std::memory_order_relaxed) - 1;
    CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>* cb =
        buffer_.load(std::memory_order_relaxed);


    bottom_.store(bottom, std::memory_order_relaxed);
    //__tsan_release(&bottom_);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    uint_fast64_t top = top_.load(std::memory_order_relaxed);
    //__tsan_acquire(&top_);
    std::pair<std::coroutine_handle<>, TaskLifeTime> return_handle;

    if (top <= bottom) {
      // queue is not empty
      return_handle = cb->get(bottom);
      if (top == bottom) {
        // last element in the qeueu, top == bottom means queue is empty and we are trying to
        // steal the last element
        if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                          std::memory_order_relaxed)) {
          // failed to pop last element, other thread stole it
          // by incrementing top
          bottom_.store(bottom + 1, std::memory_order_relaxed);
          return {};
        }
        bottom_.store(bottom + 1, std::memory_order_relaxed);
      }
    } else {
      // queue is empty
      bottom_.store(bottom + 1, std::memory_order_relaxed);
      return {};
    }

    // qeueu was not empty and we managed to pop the element
    return return_handle.first;
  }

  // does not guarantee that the value is correct
  uint_fast64_t get_buffer_size() {
    return buffer_.load(std::memory_order_relaxed)->size();
    // return std::atomic_load_explicit(&buffer_, std::memory_order_relaxed)->size();
  }

  // Returns current underlying buffer.
  // Do not use this in multithreaded environment. 
  CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>& get_underlying_buffer() {
    return *buffer_.load();
  }

 private:
  alignas(hardware_destructive_interference_size) std::atomic<uint_fast64_t> top_;
  alignas(hardware_destructive_interference_size) std::atomic<uint_fast64_t> bottom_;

  //std::atomic<uint_fast64_t> top_;
  //std::atomic<uint_fast64_t> bottom_;

  std::atomic<CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>*> buffer_;
  std::vector<CircularBuffer<std::pair<std::coroutine_handle<>, TaskLifeTime>>*> old_buffers_;
};

} // namespace detail
} // namespace coros

#endif  // COROS_INCLUDE_DEQUE_H_
