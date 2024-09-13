#ifndef COROS_INCLUDE_START_BARRIER_H_
#define COROS_INCLUDE_START_BARRIER_H_

#include <condition_variable>

namespace coros {
namespace detail {


// This barrier is used with StartTask. Once the StartTask
// finishes its execution the main thread is notified. 
// In case of async the barrier is checked once the user calls
// wait on the StartingTask.
class StartingBarrier {
 public:
  StartingBarrier(){};
  ~StartingBarrier(){};

  void wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return flag_; });
  }

  // Signals once the task is done.
  void notify() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      flag_ = true;
      cv_.notify_all();
    }
  }

  bool is_set () {
    std::lock_guard<std::mutex> lock(mtx_);
    return flag_;
  }

 private:
  bool flag_ = false;
  std::condition_variable cv_;
  std::mutex mtx_;
};

} // namespace detail
} // namespace coros

#endif // COROS_INCLUDE_START_BARRIER_H_
