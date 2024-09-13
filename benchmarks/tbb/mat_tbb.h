#pragma once

#include <chrono>
#include <iostream>
#include <atomic>

extern int g_thread_num;

void matmul(std::atomic<int> *a, std::atomic<int> *b, std::atomic<int> *c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        for (int k = 0; k < n; k++) {
          c[i * N + j].fetch_add(
            a[i * N + k].load(std::memory_order::relaxed) * b[k * N + j].load(std::memory_order::relaxed),
            std::memory_order::relaxed);
        }
      }
    }
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    tbb::parallel_invoke(
      [&]() { matmul(a, b, c, k, N); },
      [&]() { matmul(a + k, b + k * N, c, k, N); },
      [&]() { matmul(a, b + k, c + k, k, N); },
      [&]() { matmul(a + k, b + k * N + k, c + k, k, N); },
      [&]() { matmul(a + k * N, b, c + k * N, k, N); },
      [&]() { matmul(a + k * N + k, b + k * N, c + k * N, k, N); },
      [&]() { matmul(a + k * N, b + k, c + k * N + k, k, N); },
      [&]() { matmul(a + k * N + k, b + k * N + k, c + k * N + k, k, N); }
    );
  }
}

int bench_tbb_matmul() {
  int N = 1024;
  std::atomic<int> *A = new std::atomic<int>[N * N];
  std::atomic<int> *B = new std::atomic<int>[N * N];
  std::atomic<int> *C = new std::atomic<int>[N * N];

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i * N + j] = 1;
      B[i * N + j] = 1;
      C[i * N + j] = 0;
    }
  }

  int result_time = -1;
  
  tbb::task_arena arena(4);

  auto start = std::chrono::high_resolution_clock::now();

  arena.execute([&]{
    matmul((std::atomic<int>*)A, (std::atomic<int>*)B, (std::atomic<int>*)C, N, N);
  });


  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  result_time = duration.count();


  bool done = false;
  for (int i = 0; i < N && !done; i++) {
    for (int j = 0; j < N && !done; j++) {
      if ( C[i * N + j] != N) {
        std::cout << "Wrong result OpenMP matmul : " << std::endl;
        done = true;
      }
    }
  }

  return result_time;
}
