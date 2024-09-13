#include <iostream>

#include "start_tasks.h"
#include "enqueue_tasks.h"

std::atomic<int> counter = 0;

coros::Task<void> add_one() {
  counter++;
  co_return;
}

coros::Task<void> increase_counter(coros::ThreadPool& tp) {

  // The temporary task object is destroyed once the 
  // task completes.
  //
  // Enqueued tasks cannot be awaited.
  coros::enqueue_tasks(add_one());
  
  // This overload enqueues the task into a different
  // threadpool queue.
  coros::enqueue_tasks(tp, add_one());

  co_return;
}


int main () {
  coros::ThreadPool tp{2};
  coros::ThreadPool tp2{2};

  coros::Task<void> t = increase_counter(tp2);

  coros::start_sync(tp, t);
  
  // Given that start_sync only waits for the foo() task, we have no 
  // guarantee that the counter will be 2. It can be 0, 1 or 2, depending
  // on the order of execution of individual tasks.
  std::cout << "counter value : " << counter << std::endl;
}
