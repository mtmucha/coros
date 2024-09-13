#include <fstream>
#include <functional>
#include <iostream>

#include <tbb/tbb.h>

#include "fib_tbb.h"
#include "mat_tbb.h"

int g_thread_num;

void make_test(std::string file_name, std::function<int()> bench_func) {
  std::ofstream outFile(file_name);
  if (outFile.is_open()) {
    for (int i = 0; i < 50; i++) {
      outFile << bench_func() << std::endl;
    }
    outFile.close();
  } else {
    std::cerr << "Unable to open file for writing" << std::endl;
  }
}

int main(int argc, char const *argv[]) {

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <integer>" << std::endl;
  }

  g_thread_num = std::atoi(argv[1]);

  make_test("onetbb_fib_" + std::to_string(g_thread_num) + ".txt", bench_tbb_fib);
  make_test("onetbb_matmul_" + std::to_string(g_thread_num) + ".txt", bench_tbb_matmul);

  return 0;
}

