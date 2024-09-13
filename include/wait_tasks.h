#ifndef COROS_INCLUDE_WAIT_TASKS_H_
#define COROS_INCLUDE_WAIT_TASKS_H_

#include <atomic>
#include <vector>

#include "task_life_time.h"
#include "wait_barrier.h"
#include "task.h"
#include "thread_pool.h"

#include "constructor_counter.hpp"

namespace coros {
namespace detail {


// TODO: can do some concept for awaitable types
template<typename BarrierType>
class WaitTask;

template<typename BarrierType>
class WaitTaskPromise {
 public:
  // TODO: look at exceptions
  WaitTask<BarrierType> get_return_object();

  std::suspend_always initial_suspend() noexcept { return {}; }

  // when tasks finishes we need to decrease the counter
  // on the barrier so we can resume the awaiting task
  auto final_suspend() noexcept {
    class DecreaseCounterAwaiter {
     public:
      // TODO : remove it so it can be may be trivially constructible ?
      // DecreaseCounterAwaiter(WaitTaskPromise* p) noexcept : finished_task_promise(p) {}

      constexpr bool await_ready() noexcept { return false; }

      std::coroutine_handle<> await_suspend(
          [[maybe_unused]] std::coroutine_handle<> currently_suspended) noexcept {
        // TODO: make decrement_and_resume noexcept
        return finished_task_promise->barrier_->decrement_and_resume();
      }

      void await_resume() noexcept {}

      WaitTaskPromise* finished_task_promise;
    };

    return DecreaseCounterAwaiter{this};
  }

  void return_void() {}

  // TODO: maybe change approach
  void unhandled_exception() const { std::rethrow_exception(std::current_exception()); }

  void set_barrier(BarrierType* bar) noexcept { barrier_ = bar; }

  BarrierType& get_barrier() const noexcept { return *barrier_; }

 private:
  BarrierType* barrier_;
};

// Needs to be defined when running tests. Allows for checking the number
// of task instances alive.
#ifdef COROS_TEST_
  #define INSTANCE_COUNTER_WAIT_TASK_ : private coros::test::InstanceCounter<WaitTask<BarrierType>>
#else
  #define INSTANCE_COUNTER_WAIT_TASK_
#endif

// Object representing a wait WaitTask, similar to Task object. This object
// is used to manipulate WaitTask and the promise/coroutine state associated 
// with it.
// RAII object.
template<typename BarrierType>
class WaitTask INSTANCE_COUNTER_WAIT_TASK_ {
 public:
  // need a public coroutine::promise_type for coroutine traits
  // to deduce a promsie type
  using promise_type = WaitTaskPromise<BarrierType>;

  template<typename T = WaitTask>
  static std::enable_if_t<std::is_base_of_v<coros::test::InstanceCounter<WaitTask>, T>, std::size_t>
  instance_count() {
    return coros::test::InstanceCounter<WaitTask>::instance_count();
  }

  WaitTask(std::coroutine_handle<promise_type> handle) noexcept : task_handle_(handle){};

  WaitTask(const WaitTask& other) = delete;
  WaitTask& operator=(const WaitTask& other) = delete;

  WaitTask(WaitTask&& other) noexcept {
    task_handle_ = other.task_handle_;
    other.task_handle_ = nullptr;
  }

  WaitTask& operator=(WaitTask&& other) noexcept {
    task_handle_ = other.task_handle_;
    other.task_handle_ = nullptr;
    return *this;
  }
  
  // Need to keep the if statement in case the task is moved,
  // we do not want to destroy the state. RAII 
  ~WaitTask() {
    if (task_handle_) {
      task_handle_.destroy();
    }
  }

  std::coroutine_handle<promise_type> get_handle() const noexcept { return task_handle_; }

  void set_barrier(BarrierType* barrier) const noexcept { task_handle_.promise().set_barrier(barrier); }

 private:
  std::coroutine_handle<promise_type> task_handle_;
};

} // namespace detail

// An awaitable that is used that pauses current task, schedules individual tasks
// for execution into and threadpool.
//
// task<void> foo() {
//   auto t1 = task1();
//   auto t2 = task2();
//
//   co_await wait_for_tasks(t1, t2); // Here awaitable is constructed
//
//   co_return;
// }
//
// Once all tasks are done the parent task foo, is resumed. It is resumed on the 
// pool where t1 and t2 were executed.
template <typename Container>
class WaitTasksAwaitable {
 public:

  using TaskType = typename Container::value_type;

  auto operator co_await() {
    struct Awaiter {
     public:

      // This should be changed because task can be finished before we start executing.
      // TODO : Changed only if we do async waiting
      constexpr bool await_ready() const noexcept { return false; }

      // Suspends current coroutine, sets barrier for all tasks, 
      // sets continuation for the barrier(currently suspended coroutine) and 
      // adds tasks to thread pool for execution.
      void await_suspend(std::coroutine_handle<> currently_suspended) noexcept {
        awaitable_->barrier_.set_continuation(currently_suspended);

        for (auto& task : awaitable_->tasks_) {
          task.set_barrier(&(awaitable_->barrier_));
          awaitable_->tp_.add_task({task.get_handle(), detail::TaskLifeTime::SCOPE_MANAGED});
        }
      }

      void await_resume() noexcept {}

      WaitTasksAwaitable* awaitable_;
    };
    return Awaiter{this};
  }

  Container& get_tasks() noexcept { return tasks_; }              

  ThreadPool& tp_;
  Container tasks_;
  detail::WaitBarrier barrier_;
};

template <typename ThreadPool>
class WaitTasksAwaitableVector {
 public:

  auto operator co_await() {
    struct Awaiter {
     public:

      // This should be changed because task can be finished before we start executing.
      // TODO : Changed only if we do async waiting
      constexpr bool await_ready() const noexcept { return false; }

      // Suspends current coroutine, sets barrier for all tasks, 
      // sets continuation for the barrier(currently suspended coroutine) and 
      // adds tasks to thread pool for execution.
      void await_suspend(std::coroutine_handle<> currently_suspended) noexcept {
        awaitable_->barrier_.set_continuation(currently_suspended);

        for (auto& task : awaitable_->tasks_) {
          task.set_barrier(&(awaitable_->barrier_));
          awaitable_->tp_.add_task({task.get_handle(), detail::TaskLifeTime::SCOPE_MANAGED});
        }
      }

      void await_resume() noexcept {}

      WaitTasksAwaitableVector* awaitable_;
    };
    return Awaiter{this};
  }

  std::vector<detail::WaitTask<detail::WaitBarrier>>& get_tasks() noexcept { return tasks_; }              

  ThreadPool& tp_;
  std::vector<detail::WaitTask<detail::WaitBarrier>> tasks_;
  detail::WaitBarrier barrier_;
};


template <typename Container>
class WaitTasksAwaitableAsync {
 public:

  WaitTasksAwaitableAsync(coros::ThreadPool& tp,
                             Container&& tasks) 
      : tp_(tp),
        tasks_(std::move(tasks)),
        barrier_{std::make_shared<detail::WaitBarrierAsync>(tasks_.size())} {
    // Set barrier in tasks and schedule individual tasks.
    barrier_->set_barrier_pointer(&barrier_);
    for(auto& task : tasks_) {
      task.set_barrier(barrier_.get());
      tp_.add_task({task.get_handle(), detail::TaskLifeTime::SCOPE_MANAGED});
    }
  }

  auto operator co_await() {
    struct Awaiter {
     public:
      
      // Optimization when all tasks are finished, we do not need
      // to suspend.
      constexpr bool await_ready() const noexcept {  
        if (this->awaitable_->barrier_->remaining_tasks.load() == 0) {
          // All tasks are finished 
          return true;
        } else {
          return false;
        }
      }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<> currently_suspended) noexcept {
        std::shared_ptr<detail::WaitBarrierAsync> barrier = std::atomic_load(&(awaitable_->barrier_));
        // Once we store the continuation into the barrier, 
        // there is no guarantee that the awaiter is still alive, 
        // so we need to store the pointer to the barrier in function stack.
        barrier->continuation.store(currently_suspended);
        barrier->handle_ready_.store(true);
        int remaining_tasks = barrier->remaining_tasks.load();

        if (remaining_tasks >= 1) {
          // At least on tasks hasn't decremented the barrier. We can be 
          // sure that the last tasks reads the set handle, given the
          // total order, sequential consistency.
          return std::noop_coroutine();
        } else {
          bool expected = true;
          if (barrier->handle_ready_.compare_exchange_strong(
                expected, false)) {
            // If continuation contains a nullptr, it means that the last task
            // took the continuation and set the nullptr. We can be sure
            // that the coroutine will be resumed from the last task.
            // return std::noop_coroutine();
            return currently_suspended;
          } else {
            // Last task did not manage to get the stored handle, we need to resume
            // the current coroutine.
            // return currently_suspended;
            return std::noop_coroutine();
          }
        }
      }

      void await_resume() noexcept {}

      WaitTasksAwaitableAsync* awaitable_;
    };
    return Awaiter{this};
  }

  Container& get_tasks() noexcept { return tasks_; }              

  ThreadPool& tp_;
  Container tasks_;
  // With shared pointer we have guarantee that the barrier is destroyed
  // only once the coroutine is resumed and the awaiter is destroyed and 
  // when the await_suspend function ends. 
  // This ensures that the barrier lives long enough for the synchronizatoin
  // between tasks and that are awaited and the parent coroutine.
  // std::atomic<std::shared_ptr<SharedBarrierAsync>> barrier_;
  std::shared_ptr<detail::WaitBarrierAsync> barrier_;
};


// Suspends the current coroutine and waits for tasks from different pool.
template <typename Container>
class WaitTasksPoolAwaitable {
 public:

  auto operator co_await() {
    struct Awaiter {
     public:
      Awaiter(WaitTasksPoolAwaitable* awaitable) : awaitable_(awaitable) {}

      constexpr bool await_ready() const noexcept { return false; }

      // Suspends current coroutine, sets barrier for all tasks, 
      // sets continuation for the barrier(currently suspended coroutine) and 
      // adds tasks to thread pool for execution.
      void await_suspend(std::coroutine_handle<> currently_suspended)  {
        awaitable_->barrier_.set_continuation(currently_suspended);

        for (auto& task : awaitable_->tasks_) {
          task.set_barrier(&(awaitable_->barrier_));
          awaitable_->tp_.add_task_from_outside({task.get_handle(), detail::TaskLifeTime::SCOPE_MANAGED});
        }
      }

      // TODO : returning multiple values has to be done here.
      void await_resume() {}

     private:
      WaitTasksPoolAwaitable* awaitable_;
    };
    return Awaiter(this);
  }

  Container& get_tasks() noexcept { return tasks_; }              

  ThreadPool& tp_;
  Container tasks_;
  detail::WaitBarrier barrier_;
};

namespace detail{

template<typename BarrierType>
inline WaitTask<BarrierType> WaitTaskPromise<BarrierType>::get_return_object() {
  return WaitTask<BarrierType>(std::coroutine_handle<WaitTaskPromise<BarrierType>>::from_promise(*this));
}

// Used for lvalues when the task is passed by lvalue reference.
template<typename BarrierType, typename T>
WaitTask<BarrierType> wait_task_body_ref(T& t) {
  co_await t;
}

// Used for rvalues, once this WaitTask is destroyed the task
// stored in parameter list in coroutine state is also destroyed.
// Task t lifetime is tied to this task.
template<typename BarrierType, typename T>
WaitTask<BarrierType> wait_task_body_val(T t) {
  co_await t;
}

template<typename BarrierType, typename T>
inline WaitTask<BarrierType> create_wait_task(T&& t) {
  if constexpr (std::is_reference_v<T>) {
    return wait_task_body_ref<BarrierType>(std::forward<T>(t));
  } else {
    return wait_task_body_val<BarrierType>(std::forward<T>(t));
  }
}

} // namespace detail

// TODO: maybe add a concept whether the parameter is an awaitable. So it can be 
// co_awaited.
// Constructs a vector of wait tasks and returns an awaitable
template <typename... Args>
inline auto wait_tasks(Args&&... args) {
  constexpr size_t arr_size = sizeof...(Args);
  return WaitTasksAwaitable<std::array<coros::detail::WaitTask<coros::detail::WaitBarrier>, arr_size>>{
    *thread_my_pool, 
    {detail::create_wait_task<coros::detail::WaitBarrier>(std::forward<Args>(args))...},
    {arr_size, nullptr}};
}

template <typename T>
inline auto wait_tasks(std::vector<coros::Task<T>>& tasks) {
  size_t vec_size = tasks.size();
  std::vector<coros::detail::WaitTask<coros::detail::WaitBarrier>> wait_task_vec;
  for (auto& task : tasks) {
    wait_task_vec.push_back(detail::create_wait_task<coros::detail::WaitBarrier>(task));
  }
  return WaitTasksAwaitable<std::vector<coros::detail::WaitTask<coros::detail::WaitBarrier>>>{
    *thread_my_pool, 
    std::move(wait_task_vec),
    {vec_size, nullptr}};
}

template <typename... Args>
inline auto wait_tasks_async(Args&&... args) {
  constexpr size_t arr_size = sizeof...(Args);
  return WaitTasksAwaitableAsync<
          std::array<detail::WaitTask<detail::WaitBarrierAsync>, arr_size>>(
        *thread_my_pool, 
        {detail::create_wait_task<detail::WaitBarrierAsync>(std::forward<Args>(args))...});
}

template <typename T>
inline auto wait_tasks_async(std::vector<coros::Task<T>>& tasks) {
  std::vector<coros::detail::WaitTask<coros::detail::WaitBarrierAsync>> wait_task_vec;
  for (auto& task : tasks) {
    wait_task_vec.push_back(detail::create_wait_task<coros::detail::WaitBarrierAsync>(task));
  }
  return WaitTasksAwaitableAsync<
          std::vector<detail::WaitTask<detail::WaitBarrierAsync>>>(
    *thread_my_pool,
    std::move(wait_task_vec));
}

// Moves tasks between thread pools.
template <typename... Args>
inline auto wait_tasks(coros::ThreadPool& pool, Args&&... args) {
  constexpr size_t arr_size = sizeof...(Args);
  return WaitTasksPoolAwaitable<std::array<coros::detail::WaitTask<coros::detail::WaitBarrier>, arr_size>>{
    pool,
    {detail::create_wait_task<coros::detail::WaitBarrier>(std::forward<Args>(args))...},
    {arr_size, nullptr}};
}

template <typename T>
inline auto wait_tasks(coros::ThreadPool& pool,
                           std::vector<coros::Task<T>>& tasks) {
  size_t vec_size = tasks.size();
  std::vector<coros::detail::WaitTask<coros::detail::WaitBarrier>> wait_task_vec;
  for (auto& task : tasks) {
    wait_task_vec.push_back(detail::create_wait_task<coros::detail::WaitBarrier>(task));
  }
  return WaitTasksPoolAwaitable<std::vector<coros::detail::WaitTask<coros::detail::WaitBarrier>>>{
    pool, 
    std::move(wait_task_vec),
    {vec_size, nullptr}};
}

} // namespace coros


#endif // COROS_INCLUDE_WAIT_TASKS_H_
