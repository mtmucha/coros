#ifndef COROS_INCLUDE_ENQUEUE_TASK_H_
#define COROS_INCLUDE_ENQUEUE_TASK_H_

#include <coroutine>

#include "task.h"
#include "thread_pool.h"

namespace coros {

class NoWaitTaskPromise;

// Destroys tasks that is suspended through this awaiter.
// Used in final_suspend in NoWaitTask.
class DestroyTaskAwaiter {
 public:
  DestroyTaskAwaiter() {};
  ~DestroyTaskAwaiter() {};

  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<NoWaitTaskPromise> handle) noexcept {
    handle.destroy();
  }

  void await_resume() noexcept {}

 private:
};

// Wrapper around task, that destroys its own coroutine state once completed.
// Also destroyes the task it wraps, if passed as value parameter.
class NoWaitTask {
 public:
  using promise_type = NoWaitTaskPromise;

  NoWaitTask() : handle(nullptr){};

  NoWaitTask(std::coroutine_handle<promise_type> handle) : handle(handle){};

  NoWaitTask(const NoWaitTask& other) = delete;

  NoWaitTask(NoWaitTask&& other) noexcept {
    if (this != &other) {
      handle = other.handle;
      other.handle = nullptr;
    }
  }

  NoWaitTask& operator=(const NoWaitTask& other) = delete;

  NoWaitTask& operator=(NoWaitTask&& other) noexcept {
    if (this != &other) {
      handle = other.handle;
      other.handle = nullptr;
    }
    return *this;
  }

  ~NoWaitTask() { }

  std::coroutine_handle<promise_type> get_handle() { return handle; }

 private:
  std::coroutine_handle<promise_type> handle;
};

#ifdef COROS_TEST_
  #define PROMISE_INSTANCE_COUNTER_ : private coros::test::InstanceCounter<NoWaitTaskPromise>
#else
  #define PROMISE_INSTANCE_COUNTER_ 
#endif

class NoWaitTaskPromise PROMISE_INSTANCE_COUNTER_ {
 public:

  auto get_return_object() {
    return NoWaitTask{std::coroutine_handle<NoWaitTaskPromise>::from_promise(*this)};
  }

  template<typename T = NoWaitTaskPromise>
  static std::enable_if_t<std::is_base_of_v<coros::test::InstanceCounter<NoWaitTaskPromise>, T>, std::size_t> instance_count() {
    return coros::test::InstanceCounter<NoWaitTaskPromise>::instance_count();
  }


  std::suspend_always initial_suspend() noexcept { return {}; }

  DestroyTaskAwaiter final_suspend() noexcept {
    return DestroyTaskAwaiter{};
  };

  // This type of task does not return anything. 
  void return_void() {}

  void unhandled_exception() { std::rethrow_exception(std::current_exception()); }

 private:
};

// Accept task by value, because it will be destroyed once completed.
template <typename T>
inline NoWaitTask create_NoWaitTask(Task<T> t) {
  co_await t;
  co_return;
}

inline NoWaitTask create_NoWaitTask() {
  co_return;
}

// Accept an r-value reference for the task, so we can move it/construct it in
// the NoWaitTask argument, so it can be destroyed with the NoWaitTask.
template <typename... Args>
requires (std::is_rvalue_reference_v<Args&&> && ...)
inline void enqueue_tasks(ThreadPool& tp, Args... args) {
  ((tp).add_task_from_outside(
      {create_NoWaitTask(std::move(args)).get_handle(), 
       detail::TaskLifeTime::THREAD_POOL_MANAGED}), ...);
}

template <typename... Args>
requires (std::is_rvalue_reference_v<Args&&> && ...)
inline void enqueue_tasks(Args&&... args) {
  ((*thread_my_pool).add_task_from_outside(
      {create_NoWaitTask(std::move(args)).get_handle(), 
       detail::TaskLifeTime::THREAD_POOL_MANAGED}), ...);
}

template <typename T>
inline void enqueue_tasks(std::vector<coros::Task<T>>&& vec) {
  for(auto&& task : vec) {
    (*thread_my_pool).add_task_from_outside({create_NoWaitTask(std::move(task)).get_handle(),
                              detail::TaskLifeTime::THREAD_POOL_MANAGED});
  }
}

template <typename T>
inline void enqueue_tasks(coros::ThreadPool& tp, std::vector<coros::Task<T>>&& vec) {
  for(auto&& task : vec) {
    tp.add_task_from_outside({create_NoWaitTask(std::move(task)).get_handle(),
                              detail::TaskLifeTime::THREAD_POOL_MANAGED});
  }
}


} // namespace coros


#endif // COROS_INCLUDE_ENQUEUE_TASK_H_
