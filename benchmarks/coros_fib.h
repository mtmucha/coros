#include <chrono>
#include <iostream>

#include "start_tasks.h"
#include "thread_pool.h"
#include "wait_tasks.h"


extern int g_thread_num;

inline coros::Task<long> fib(int index) {
  if (index < 2) co_return index;

  coros::Task<long> a = fib(index - 1);
  coros::Task<long> b = fib(index - 2);

  co_await coros::wait_tasks(a, b);

  co_return *a + *b;
}


inline int bench_workstealing_fib() {
  coros::ThreadPool tp{g_thread_num};
  coros::Task<long> t = fib(30);

  auto start = std::chrono::high_resolution_clock::now();

  coros::start_sync(tp, t);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  if (*t != 832040) {
    std::cerr << "Wrong result Coros fib : " << *t << std::endl;
  }

  return duration.count();
}
