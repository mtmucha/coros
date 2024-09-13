#include <iostream>

#include "start_tasks.h"


coros::Task<int> add_value(int val) {
  co_return val + 1;
}

coros::Task<int> throw_exception(int val) {
  throw std::bad_alloc();
  co_return val + 1;
}


int main () {
  // Construction of the thread pool, number of desired threads can
  // be specified.
  coros::ThreadPool tp{2};
  
  // Construction of the task objects. This objects need to
  // outlive the execution time (cannot be destroyed before the
  // execution finishes). They hold the final value.
  coros::Task<int> t1 = add_value(41);
  coros::Task<int> t2 = throw_exception(41);
  
  // Starting tasks in the specified thread pool. This method
  // blocks until the tasks are finished. There is also an 
  // async version of this method.
  coros::start_sync(tp, t1, t2);
  
  // If a task successfully finishes and stores the value
  // has_value() returns true.
  if (t1.has_value()) {
    // Operator *t1 allows for accessing the stored value inside the task.
    std::cout << "t1 value : " << *t1 << std::endl;
  } else {
    // Task did not finish successfully and an exception
    // has been thrown, which is stored inside the coros::Task
    // object. It can be retrieved with error() method, see below.
  }

  if (t2.has_value()) {
    std::cout << "t2 value : " << *t1 << std::endl;
  } else {
    // In case an unexpected value has been stored, in this case
    // an exception. It can be accessed with error() method.

    // For example it can be rethrown and processed. 
    try {
      std::rethrow_exception(t2.error());
    }
    catch (const std::exception& e) { 
      std::cerr << "Exception caught from task: " << e.what() << std::endl;
    }
  }

}
