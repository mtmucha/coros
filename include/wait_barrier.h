#ifndef COROS_INCLUDE_WAIT_BARRIER_H_
#define COROS_INCLUDE_WAIT_BARRIER_H_

#include <atomic>
#include <coroutine>
#include <memory>

namespace coros {
namespace detail {

// Barrier is constructed with number of tasks that need to finish
// and and a coroutine handle of the task that waits for them. Once
// all tasks are finished the last task takes the handle and resumes
// the waiting task(which is suspended).
class WaitBarrier {
 public:

  std::coroutine_handle<> decrement_and_resume() noexcept {
    int remaining = remaining_tasks.fetch_sub(1, std::memory_order_acq_rel);
    //int remaining = remaining_tasks.fetch_sub(1, std::memory_order::seq_cst);
    if (remaining == 1) {
      return continuation;
    } else {
      return std::noop_coroutine();
    }
  }

  uint_fast64_t get_counter() {
      return remaining_tasks;
  }

  void set_continuation(std::coroutine_handle<> handle) {
      continuation = handle;
  }

  std::coroutine_handle<> get_continuation () {
      return continuation;
  }

  std::atomic<uint_fast64_t> remaining_tasks;
  // coroutine to resume, when the all tasks are done
  std::coroutine_handle<> continuation;
};

class WaitBarrierAsync {
 public: WaitBarrierAsync(int task_number) noexcept 
    : remaining_tasks(task_number), continuation(nullptr),
      handle_ready_(false) {}
  
  // Each thread needs to make a copy of the shared_ptr to instance of this object.
  // This prevents a scenario where compare_exchange_strong is called on deleted
  // barrier. This can happen : 
  //
  //  1. Each task decrements the barrier. This allows the awaiter in other
  //  thread to resume the coroutine. There is not need to wait. When all tasks
  //  decremented the barrier, signalling they are done.
  //  2. Last tasks tries to check wheter it should resume the coroutine or
  //  not through compare_exchange_strong. However, the barrier is already
  //  destroyed.
  //
  //  Task cannot know if itself is the last task until it decrements the barrier.
  //  Therefore every task needs to make the copy of the pointer before decrement,
  //  keeping the barrier alive.
  std::coroutine_handle<> decrement_and_resume() noexcept {
    std::shared_ptr<WaitBarrierAsync> barrier = std::atomic_load(barrier_p_);
    int remaining = remaining_tasks.fetch_sub(1);
    if (remaining == 1) {
      // Last task.
      bool expected = true;
      if (handle_ready_.compare_exchange_strong(expected, false)) {
        // This tasks resumes the coroutine and sets flag for the awaiter, so it 
        // doesn't resume the coroutine. 
        return continuation.load();
      } else {
        // Flag is not set. The awaiter will resume the coroutine.
        return std::noop_coroutine();
      }
    } else {
      return std::noop_coroutine();
    }
  }

  uint_fast64_t get_counter() {
      return remaining_tasks.load();
  }

  void set_continuation(std::coroutine_handle<> handle) {
      continuation = handle;
  }

  void set_barrier_pointer(std::shared_ptr<WaitBarrierAsync>* barrier_p) {
    barrier_p_ = barrier_p;
  }

  std::coroutine_handle<> get_continuation () {
      return continuation;
  }

  std::atomic<uint_fast64_t> remaining_tasks;
  // Coroutine to resume, when the all tasks are done. Initialized to nullptr
  std::atomic<std::coroutine_handle<>> continuation;
  std::shared_ptr<WaitBarrierAsync>* barrier_p_;
  std::atomic<bool> handle_ready_; 
};

} // namespace detail
} // namespace coros


#endif // COROS_INCLUDE_WAIT_BARRIER_H_
