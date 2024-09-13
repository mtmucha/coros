#include <fstream>

int g_thread_num;

#include "coros_fib.h"
#include "coros_mat.h"

#include "openmp_fib.h"
#include "openmp_mat.h"

#include <functional>

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

  make_test("openmp_fib_" + std::to_string(g_thread_num) + ".txt", bench_openmp_fib);
  make_test("openmp_matmul_" + std::to_string(g_thread_num) + ".txt", bench_openmp_matmul);

  make_test("workstealing_fib_" + std::to_string(g_thread_num) + ".txt", bench_workstealing_fib);
  make_test("workstealing_matmul_" + std::to_string(g_thread_num) + ".txt", bench_workstealing_matmul);
 
  return 0;
}
