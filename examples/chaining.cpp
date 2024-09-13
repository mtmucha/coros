#include <iostream>

#include "chain_tasks.h"
#include "start_tasks.h"

coros::Task<int> add_two(int val) {
  co_return val + 2;
}

coros::Task<int> multiply_by_six(double val) {
  co_return val * 6;
}

coros::Task<void> accept_int(int val) {
  co_return;
}

coros::Task<void> accept_nothing() {
  co_return;
}

coros::Task<int> return_three() {
  co_return 3;
}

coros::Task<int> compute_value() {

  // Each task must accept at most one argument
  //
  // Each subsequent task's parameter must be
  // constructible from the previous task return value.
  std::expected<int, std::exception_ptr> res = 
    co_await coros::chain_tasks(3).and_then(add_two)
                                  .and_then(add_two)
                                  .and_then(multiply_by_six);
  if (res.has_value()) {
    // Return computed value.
    co_return *res;
  } else {
    // In case the chain did not sucessfuly finish.
    co_return -1;
  }
}

coros::Task<int> compute_value_void() {
  
  // The value is "lost"  in the chain and resulting
  // expected is of type void.
  std::expected<void, std::exception_ptr> res = 
    co_await coros::chain_tasks(3).and_then(add_two)
                                  .and_then(add_two)
                                  .and_then(multiply_by_six)
                                  .and_then(accept_int)
                                  .and_then(accept_nothing)
                                  .and_then(accept_nothing);
  if (res.has_value()) {
    // Chain sucessfuly finished.
    co_return 1;
  } else {
    // Chain did not sucessfully finished.
    co_return -1;
  }
}

coros::Task<int> compute_value_task() {
  // It is possible to pass a unfinished/unexecuted task
  // object into the chain_tasks. The starting task will
  // be executed and then all the folowing tasks.
  std::expected<int, std::exception_ptr> res = 
    co_await coros::chain_tasks(return_three()).and_then(add_two)
                                               .and_then(add_two)
                                               .and_then(multiply_by_six);
  if (res.has_value()) {
    // Chain sucessfuly finished.
    co_return 1;
  } else {
    // Chain did not sucessfully finished.
    co_return -1;
  }
}

coros::Task<int> compute_value_finished_task() {
  coros::Task<int> task = return_three();

  coros::wait_tasks(task);

  // Chain can also be started with a finished task.
  std::expected<int, std::exception_ptr> res = 
    co_await coros::chain_tasks(*task).and_then(add_two)
                                      .and_then(add_two)
                                      .and_then(multiply_by_six);
  if (res.has_value()) {
    // Chain sucessfuly finished.
    co_return 1;
  } else {
    // Chain did not sucessfully finished.
    co_return -1;
  }
}


int main () {
  coros::ThreadPool tp(2);

  coros::Task<int> t1 = compute_value();
  coros::Task<int> t2 = compute_value_void();
  coros::Task<int> t3 = compute_value_task();
  coros::Task<int> t4 = compute_value_finished_task();

  coros::start_sync(tp, t1, t2, t3, t4);

  std::cout << "value stored in t1 : " << *t1 << std::endl;
  std::cout << "value stored in t2 : " << *t2 << std::endl;
  std::cout << "value stored in t3 : " << *t3 << std::endl;
  std::cout << "value stored in t4 : " << *t4 << std::endl;
}
