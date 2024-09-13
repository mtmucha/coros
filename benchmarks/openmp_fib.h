#pragma once

#include <chrono>
#include <iostream>

#include "omp.h"

extern int g_thread_num;

long fibonacci(int n) {
  if (n < 2) return n;

  long a, b;

  #pragma omp task shared(a)
  a = fibonacci(n - 1);

  #pragma omp task shared(b)
  b = fibonacci(n - 2);

  #pragma omp taskwait

  return a + b;
}

int bench_openmp_fib() {
  int n = 30;
  int result = -1;
  int result_time = -1;
  omp_set_num_threads(g_thread_num);

  #pragma omp parallel
  {
    #pragma omp single
    {
      auto start = std::chrono::high_resolution_clock::now();

      result = fibonacci(n);

      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      result_time = duration.count();
    }
  }

  if (result != 832040) {
    std::cerr << "Wrong result OpenMP fib: " << result << std::endl;
  }

  return result_time;
}
