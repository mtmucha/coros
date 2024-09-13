#ifndef COROS_INCLUDE_THREAD_POOL_H_
#define COROS_INCLUDE_THREAD_POOL_H_

#include <coroutine>
#include <thread>
#include <vector>
#include <random>


#include "deque.h"
#include "concurrentqueue.h"

#ifdef COROS_TEST_DEQUE_
#include "test_deque.h"
#endif


namespace coros {

class ThreadPool;

// If this macro is defined the test Deque, using mutex is used
#ifdef COROS_TEST_DEQUE_
  #define DEQUE_ test::TestDeque 
#else
  #define DEQUE_ coros::detail::Dequeue
#endif // DEBUG

// Pointer to thread's own task deque.
inline thread_local DEQUE_*  thread_my_tasks;
// inline thread_local Dequeue* thread_my_tasks;
// inline thread_local TestDeque* thread_my_tasks;
// Each thread has a pointer to its own generator for generating
// random index from which other threads are checked for stealing a task.  
inline thread_local std::mt19937* thread_gen;
// Pointer to a thread pool to which the thread belongs to.
inline thread_local ThreadPool*  thread_my_pool;

// Holds individual threads and their task queues.
// TODO : delete constructors (move and copy)
class ThreadPool {
 public:
  ThreadPool(int thread_count);

  void add_task(std::pair<std::coroutine_handle<>, detail::TaskLifeTime>&& task);
  
  void add_task_from_outside(std::pair<std::coroutine_handle<>, detail::TaskLifeTime>&& handle);

  std::coroutine_handle<> get_task();

  void stop_threads();

  auto schedule_this_task();

  ~ThreadPool();

 private:
  void run();

  std::atomic<bool> threads_stop_executing_ = false;
  std::vector<std::thread> workers_;

  // Deques for each worker thread.
  std::vector<DEQUE_> worker_queues_;
  // Used to get tasks to queue task from "outside" of ThreadPool.
  moodycamel::ConcurrentQueue<std::pair<std::coroutine_handle<>, detail::TaskLifeTime>> new_tasks_;

  std::random_device rd_;
  std::vector<std::mt19937> gens_;
  std::uniform_int_distribution<> dists_;
};

inline ThreadPool::ThreadPool(int thread_count)
    : worker_queues_(std::vector<DEQUE_>(thread_count)), 
      gens_(thread_count), 
      dists_(0, thread_count - 1) {
  workers_.reserve(thread_count);
  for (int i = 0; i < thread_count; i++) {
    DEQUE_* deque = &worker_queues_[i];
    std::mt19937* gen = &gens_[i];
    workers_.emplace_back([this, deque, gen]() {
        thread_my_tasks = deque;
        thread_gen = gen;
        thread_my_pool = this;
        this->run();
    });
  }
}


// Individual threads use this method to add tasks into their own deque. 
inline void ThreadPool::add_task(std::pair<std::coroutine_handle<>, detail::TaskLifeTime>&& handle) {
  thread_my_tasks->pushBottom(std::move(handle));
}

// Used to add tasks from "outside"(not from within of the threadpool) not from individual threads. Tasks are pushed
// into a different queue from which individual threads can take them as a new work.
// New tasks cannot be pushed directly into the work-stealing dequeue.
inline void ThreadPool::add_task_from_outside(std::pair<std::coroutine_handle<>, detail::TaskLifeTime>&& handle) {
  new_tasks_.enqueue(std::move(handle));
}

// Main method run by each thread. Individual threads check for available work. 
// New tasks are started through coroutine handle, by calling resume() on
// the handle.
inline std::coroutine_handle<> ThreadPool::get_task() {
  // Worker tries to get task from its own queue. If there is a tasks
  // in its own deuque, the handle is returned. 
  auto task = thread_my_tasks->popBottom();
  if (task.has_value()) [[likely]] {
    return task.value();
  }

  // In case the thread does not have tasks in its own deque, it
  // tries to steal from other threads.
  // Random index is generate for even distribution.
  int random_index = dists_(*thread_gen);
  for (size_t i = 0; i < worker_queues_.size(); i++) {
    int index = (random_index + i) % worker_queues_.size();
    // Skip my own queue (can't steal from myself)
    if (&worker_queues_[index] == thread_my_tasks) [[unlikely]] continue;
    task = worker_queues_[index].steal();
    if (task.has_value()) {
      return task.value();
    }
  }

  // In case a thread does not have a task in its own deque nor steals 
  // a task from another thread, we checked new_task qeueu, which is shared among
  // all threads, for new work.
  std::pair<std::coroutine_handle<>, detail::TaskLifeTime> new_task;
  if (new_tasks_.try_dequeue(new_task)) {
    return new_task.first; 
  }
  
  // If there is no work, the std::noop_coroutine_handle is returned. Resuming this
  // handle does nothing, which repeats the loop inside the run() method.
  return std::noop_coroutine();
}

// TODO: Implement thread yielding in case there is no work for an extended duration to 
// not waste CPU.
// Main loop run by each thread.
inline void ThreadPool::run() {
  while (!threads_stop_executing_.load(std::memory_order::acquire)) [[likely]] {
    // Takes tasks and resumes it. If the qeuue is empty, 
    // it will return noop_coroutine.
    this->get_task().resume();
  }
}

inline ThreadPool::~ThreadPool() { 
  stop_threads(); 

  // Should be safe to use size_approx here, because 
  // all thread are stopped. 
  size_t size = new_tasks_.size_approx();
  std::vector<std::pair<std::coroutine_handle<>, detail::TaskLifeTime>> tasks(size);
  new_tasks_.try_dequeue_bulk(tasks.data(), size);

  for (size_t i = 0; i < size; i++) {
    if (tasks[i].second == detail::TaskLifeTime::THREAD_POOL_MANAGED) {
      tasks[i].first.destroy();
    }
  }
}

// request stop for all threads
// TODO : destruction of tasks
inline void ThreadPool::stop_threads() {
  // TODO: Check for weaker synchronizatoin
  threads_stop_executing_.store(true, std::memory_order::release);
  for (auto& t : workers_) {
    t.join();
  }
}

// Accept an rvalue reference for the task, so we can move it/construct it in
// the NoWaitTask argument, so it can be destroyed with the NoWaitTask.
/*template <typename T>
inline void fire_task(Task<T>&& task, ThreadPool& tp) {
  auto t = create_NoWaitTask(std::move(task));
  tp.add_task_from_outside({t.get_handle(), detail::TaskLifeTime::THREAD_POOL_MANAGED});
}*/


} // namespace coros

#endif  // COROS_INCLUDE_THREAD_POOL_H_
