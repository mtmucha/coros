#include <iostream>

#include "start_tasks.h"
#include "wait_tasks.h"


// Helper function, just for demonstration.
coros::Task<int> multiply(int val, int mul) {
  co_return val * mul;
}

// This tasks waits for two other tasks to finish.
coros::Task<int> generate_answer() {

  coros::Task<int> t1 = multiply(20, 2);
  
  // Suspends current execution and only resumes once the individual tasks are finished.
  //
  // Notice: with temporary task created with lambda function we cannot retrieve the value.
  co_await coros::wait_tasks(t1, 
                             [](int val, int mul) -> coros::Task<int>{
                               // This task does the same thing as the
                               // multiply task, however, we cannot retrieve
                               // value from a temporary task.
                               co_return val * mul;
                             }(20, 2));
  
  // We assume the value has been correctly stored, not checking with has_value().
  co_return *t1 + 2;
}

// Same version as a task above, except this tasks is resumed
// on another threadpool.
coros::Task<int> generate_answer_threadpool(coros::ThreadPool& tp2) {

  coros::Task<int> t1 = multiply(20, 2);
  
  // When a threadpool is specified the suspended tasks is resumed on the
  // threadpool tp2.
  co_await coros::wait_tasks(tp2,
                             t1, 
                             [](int val, int mul) -> coros::Task<int>{
                               co_return val * mul;
                             }(20, 2));
  
  // This executed on the threadpool tp2.
  co_return *t1 + 2;
}

// Waits for the tasks asynchronously.
coros::Task<int> generate_answer_async() {

  coros::Task<int> t1 = multiply(20, 2);
  
  // We store the awaitable so we can do some work and check if the tasks
  // are finished later.  
  auto awaitable = coros::wait_tasks_async(t1, 
                                           [](int val, int mul) -> coros::Task<int>{
                                             co_return val * mul;
                                           }(20, 2));
  //
  // Do some work
  //
  
  // Either suspends current tasks and resumes it later, 
  // or in case when tasks has already finished, the execution
  // simply continues without suspension.
  co_await awaitable;

  co_return *t1 + 2;
}

int main () {
  coros::ThreadPool tp{/*number_of_threads=*/2};
  coros::ThreadPool tp2{/*number_of_threads=*/2};

  coros::Task<int> wait_task = generate_answer();
  // In this tasks the suspended task and individual tasks
  // are executed on a different threadpool.
  coros::Task<int> wait_task_pool = generate_answer_threadpool(tp2);
  // Waits for tasks asynchronously.
  coros::Task<int> wait_task_async = generate_answer_async();
  
  // start_async returns a start_task 
  auto start_task = coros::start_async(tp, 
                                       wait_task, 
                                       wait_task_pool,
                                       wait_task_async);
  
  // Calling wait on the start_task, in case task are already finished
  // execution continues. Otherwise the call blocks until the tasks are finished.
  start_task.wait();

  std::cout << "value of wait_task : " << *wait_task << std::endl;
  std::cout << "value of wait_task_pool : " << *wait_task_pool << std::endl;
  std::cout << "value of wait_task_async : " << *wait_task_async << std::endl;
}
