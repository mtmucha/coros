#pragma once

#include <chrono>
#include <iostream>

extern int g_thread_num;

long fibonacci(int n) {
  if (n < 2) return n;

  long a, b;

  tbb::parallel_invoke(
    [&] { a = fibonacci(n-1); },
    [&] { b = fibonacci(n-2); }
  );

  return a + b;
}

int bench_tbb_fib() {
  int n = 30;
  int result = -1;
  int result_time = -1;
  
  tbb::task_arena arena(4);

  auto start = std::chrono::high_resolution_clock::now();

  arena.execute([&]{
    result = fibonacci(n);
  });


  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  result_time = duration.count();

  if (result != 832040) {
    std::cerr << "Wrong result OneTBB: " << result << std::endl;
  }


  return result_time;
}
