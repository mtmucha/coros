#ifndef COROS_INCLUDE_START_TASKS_H_
#define COROS_INCLUDE_START_TASKS_H_

#include <coroutine>

#include "start_barrier.h"
#include "wait_tasks.h"
#include "thread_pool.h"

namespace coros {

class StartTask;

namespace detail {

class StartTaskPromise;

class NotifyBarrierAwaiter {
 public:
  NotifyBarrierAwaiter(StartingBarrier& barrier) : barrier_(barrier){};
  ~NotifyBarrierAwaiter(){};

  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend([[maybe_unused]] std::coroutine_handle<StartTaskPromise> handle) noexcept {
    barrier_.notify();
  }

  void await_resume() noexcept {}
 private:
  StartingBarrier& barrier_;
};

class StartTaskPromise {
 public:
  friend class NotifyBarrierAwaiter;

  auto get_return_object();

  std::suspend_always initial_suspend() noexcept { return {}; }

  NotifyBarrierAwaiter final_suspend() noexcept {
    return NotifyBarrierAwaiter{barrier_};
  };

  void return_void() {}

  void unhandled_exception() { std::rethrow_exception(std::current_exception()); }

  StartingBarrier& get_barrier() { return barrier_; }

  bool is_finished() {
    return barrier_.is_set();
  }

 private:
  // barrier used to signal that the task is finished
  StartingBarrier barrier_;
};


} // namespace detail

// This tasks is used to wrap task created in main thread.
// Once the wrapped task finishes its execution, control
// is returned to this task, which notifies the barrier.
class StartTask {
 public:
  using promise_type = detail::StartTaskPromise;


  StartTask(std::coroutine_handle<promise_type> handle) : handle_(handle){};

  StartTask(const StartTask& other) = delete;

  StartTask(StartTask&& other) {
    handle_ = other.handle_;
    other.handle_ = nullptr;
  };

  StartTask operator=(StartTask&& other) = delete;
  StartTask operator=(const StartTask& other) = delete;
  
  ~StartTask() {
    if (handle_) {
      handle_.destroy();
    }
  };
  
  // Wait for the barrier
  void wait() {
    handle_.promise().get_barrier().wait();
  }

  std::coroutine_handle<promise_type> get_handle() { return handle_; }

 private:
  std::coroutine_handle<promise_type> handle_ = nullptr;
};

namespace detail {

inline auto StartTaskPromise::get_return_object() {
  return StartTask{std::coroutine_handle<StartTaskPromise>::from_promise(*this)};
}

// Lifetime of the tasks is managed by the WaitForTasksAwaiter.
// In case of an R-value the task object lives in the awaiter.
template<typename... Args>
StartTask start_task_body(Args&&... tasks) {
  co_await wait_tasks(std::forward<Args>(tasks)...);
  co_return;
}

template<typename... Args>
StartTask start_task_body_async(ThreadPool& scheduler, Args&&... tasks) {
  co_await wait_tasks(scheduler, std::forward<Args>(tasks)...);
  co_return;
}

} // namespace detail

// Allows for synchronous waiting, calls wait() on the barrier.
// Schedules the task, and waits for completion of the wrapping barrier task.
template <typename... Args>
void start_sync(ThreadPool& scheduler, Args&&... args) {
  // Wrap task into a barrier task. 
  // Tasks can only be co_awaited inside an another coroutine.
  StartTask bt = detail::start_task_body(std::forward<Args>(args)...);
  scheduler.add_task_from_outside({bt.get_handle(), detail::TaskLifeTime::SCOPE_MANAGED});
  bt.get_handle().promise().get_barrier().wait();
}

// Return a barrier task, user must call wait.
template <typename... Args>
[[nodiscard]] StartTask start_async(ThreadPool& scheduler, Args&&... args) {
  StartTask bt = detail::start_task_body_async(scheduler, std::forward<Args>(args)...);
  // At this point the R-value tasks are moved.
  // And we can sefaly move the task into a scheduler.
  bt.get_handle().resume();
  // Put task into the scheduler.
  return bt;
}

} // namespace coros

#endif // COROS_INCLUDE_START_TASKS_H_
