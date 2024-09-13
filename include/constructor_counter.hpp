#ifndef COROS_INCLUDE_INSTANCE_COUNTER_H_
#define COROS_INCLUDE_INSTANCE_COUNTER_H_

#include <cstddef>
#include <atomic>

namespace coros {
namespace test {

// Used in tests for couting the number of instances alive.
template<typename T>
class InstanceCounter {
 public:
  InstanceCounter() {
    count_.fetch_add(1, std::memory_order::relaxed);
  }

  ~InstanceCounter(){
    count_.fetch_sub(1, std::memory_order::relaxed);
  }

  static std::size_t instance_count() {
    return count_.load(std::memory_order::relaxed);
  }
   
 private:
  inline static std::atomic<std::size_t> count_ = 0;
};

// Used for checking whether the correct constructor has been used.
class ConstructorCounter {
 public:
  inline static std::atomic<int> default_constructed = 0;
  inline static std::atomic<int> copy_constructed = 0;
  inline static std::atomic<int> move_constructed = 0;
  inline static std::atomic<int> copy_assigned = 0;
  inline static std::atomic<int> move_assigned = 0;

  // Default constructor
  ConstructorCounter() {
    default_constructed.fetch_add(1, std::memory_order_relaxed);
  }

  // Copy constructor
  ConstructorCounter(const ConstructorCounter&) {
    copy_constructed.fetch_add(1, std::memory_order_relaxed);
  }

  // Move constructor
  ConstructorCounter(ConstructorCounter&&) noexcept {
    move_constructed.fetch_add(1, std::memory_order_relaxed);
  }

  // Copy assignment operator
  ConstructorCounter& operator=(const ConstructorCounter&) {
    copy_assigned.fetch_add(1, std::memory_order_relaxed);
    return *this;
  }

  // Move assignment operator
  ConstructorCounter& operator=(ConstructorCounter&&) noexcept {
    move_assigned.fetch_add(1, std::memory_order_relaxed);
    return *this;
  }

  static void clear_count() {
    ConstructorCounter::default_constructed.store(0, std::memory_order_relaxed);
    ConstructorCounter::copy_constructed.store(0, std::memory_order_relaxed);
    ConstructorCounter::move_constructed.store(0, std::memory_order_relaxed);
    ConstructorCounter::copy_assigned.store(0, std::memory_order_relaxed);
    ConstructorCounter::move_assigned.store(0, std::memory_order_relaxed);
  }
}; 

} // namespace detail
} // namespace coros


#endif  // COROS_INCLUDE_INSTANCE_COUNTER_H_
