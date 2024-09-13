#ifndef COROS_INCLUDE_TEST_DEQUE_H_
#define COROS_INCLUDE_TEST_DEQUE_H_

#include <mutex>
#include <deque>
#include <coroutine>
#include <utility>
#include <optional>

#include "task_life_time.h"

namespace coros {
namespace test {

// Deque used for testing. Utilizes mutex. 
class TestDeque {
 public:
  void pushBottom(std::pair<std::coroutine_handle<>, detail::TaskLifeTime> value) {
    std::lock_guard<std::mutex> lock(mutex_);
    deque_.push_back(value);
  }

  std::optional<std::coroutine_handle<>> popBottom() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!deque_.empty()) {
      std::pair<std::coroutine_handle<>, detail::TaskLifeTime> value = deque_.back();
      deque_.pop_back();
      return value.first;
    }
    return std::nullopt;
  }

  std::optional<std::coroutine_handle<>> steal() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!deque_.empty()) {
      std::pair<std::coroutine_handle<>, detail::TaskLifeTime> value = deque_.front();
      deque_.pop_front();
      return value.first;
    }
    return std::nullopt;
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return deque_.empty();
  }

  ~TestDeque() {
    for (auto& [handle, life_time] : deque_) {
      if (life_time == detail::TaskLifeTime::THREAD_POOL_MANAGED) {
        handle.destroy();
      }
    }
  }
  
 private:
  std::deque<std::pair<std::coroutine_handle<>, detail::TaskLifeTime>> deque_;
  std::mutex mutex_;
};

} // namespace detail
} // namespace coros

#endif // COROS_INCLUDE_TEST_DEQUE_H_
