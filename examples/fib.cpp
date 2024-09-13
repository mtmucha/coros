#include <iostream>

#include "start_tasks.h"
#include "thread_pool.h"
#include "wait_tasks.h"

coros::Task<long> fib(int n) {
  if (n < 2) co_return n;

  coros::Task<long> a = fib(n - 1);
  coros::Task<long> b = fib(n - 2);

  co_await coros::wait_tasks(a, b);

  co_return *a + *b;
}


/*long fib(int n) {
  if (n < 2) return n;

  long a  = fib(n - 1);
  long b  = fib(n - 1);



  return a + b;
}*/

int main() {
  coros::Task<long> task = fib(10);

  {
    coros::ThreadPool tp{4};
    coros::start_sync(tp, task);
  } // Once the ThreadPool goes out of scope, all worker threads are joined and destroyed.

  std::cout << "The 10th fibonacci number is : " << *task << std::endl;
}
