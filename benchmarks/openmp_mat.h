#pragma once

#include <chrono>
#include <iostream>
#include <vector>

#include "omp.h"

extern int g_thread_num;

typedef std::vector<std::vector<int>> matrix;


namespace {
void matmul(int *a, int *b, int *c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        for (int k = 0; k < n; k++) {
          #pragma omp atomic update relaxed
          c[i * N + j] += a[i * N + k] * b[k * N + j];
        }
      }
    }
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    #pragma omp task
    matmul(a, b, c, k, N);

    #pragma omp task
    matmul(a + k, b + k * N, c, k, N);

    #pragma omp task
    matmul(a, b + k, c + k, k, N);

    #pragma omp task
    matmul(a + k, b + k * N + k, c + k, k, N);

    #pragma omp task
    matmul(a + k * N, b, c + k * N, k, N);

    #pragma omp task
    matmul(a + k * N + k, b + k * N, c + k * N, k, N);

    #pragma omp task
    matmul(a + k * N, b + k, c + k * N + k, k, N);

    #pragma omp task
    matmul(a + k * N + k, b + k * N + k, c + k * N + k, k, N);

    #pragma omp taskwait  // Wait for all tasks to complete before returning
  }
}

}

int bench_openmp_matmul() {
  int N = 1024;
  int *A = new int[N * N];
  int *B = new int[N * N];
  int *C = new int[N * N];

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i * N + j] = 1;
      B[i * N + j] = 1;
      C[i * N + j] = 0;
    }
  }

  int result;
  omp_set_num_threads(g_thread_num);
  
  #pragma omp parallel
  {
    #pragma omp single
    {
      auto start = std::chrono::high_resolution_clock::now();

      matmul((int *)A, (int *)B, (int *)C, N, N);

      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      result = duration.count();
    }
  }
  
  bool done = false;

  for (int i = 0; i < N && !done; i++) {
    for (int j = 0; j < N && !done; j++) {
      if ( C[i * N + j] != N) {
        std::cout << "Wrong result OpenMP matmul : " << std::endl;
        done = true;
      }
    }
  }


  return result;
}
