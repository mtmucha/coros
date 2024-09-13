#include <chrono>
#include <iostream>

#include "thread_pool.h"
#include "wait_tasks.h"



extern int g_thread_num;

namespace {

coros::Task<void> matmul(int *a, int *b, std::atomic<int> *c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        for (int k = 0; k < n; k++) {
          c[i * N + j].fetch_add(a[i * N + k] * b[k * N + j], std::memory_order::relaxed);
        }
      }
    }
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    co_await coros::wait_tasks(
        matmul(a, b, c, k, N),
        matmul(a + k, b + k * N, c, k, N),
        matmul(a, b + k, c + k, k, N),
        matmul(a + k, b + k * N + k, c + k, k, N),
        matmul(a + k * N, b, c + k * N, k, N),
        matmul(a + k * N + k, b + k * N, c + k * N, k, N),
        matmul(a + k * N, b + k, c + k * N + k, k, N),
        matmul(a + k * N + k, b + k * N + k, c + k * N + k, k, N)
    );
  }
}

}

int bench_workstealing_matmul() {
  int N = 1024;
  int *A = new int[N * N];
  int *B = new int[N * N];
  std::atomic<int> *C = new std::atomic<int>[N * N];

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i * N + j] = 1;
      B[i * N + j] = 1;
      C[i * N + j] = 0;
    }
  }

  coros::ThreadPool tp{g_thread_num};
  coros::Task<void> t = matmul(A, B, C, N, N);

  auto start = std::chrono::high_resolution_clock::now();

  coros::start_sync(tp, t);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  bool done = false;
  for (int i = 0; i < N && !done; i++) {
    for (int j = 0; j < N && !done; j++) {
      if (C[i * N + j] != N) {
        std::cout << "Wrong result Coros matmul : " << std::endl;
        done = true;
      }
    }
  }


  return duration.count();
}
