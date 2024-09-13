cmake_minimum_required(VERSION 3.20)

project(coros_benchmark)

add_executable(
  coros_benchmark
  main.cpp
)

target_include_directories(coros_benchmark 
  PUBLIC ${CMAKE_SOURCE_DIR}/include
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(coros_benchmark PRIVATE
      -std=c++23
      -O3
      -fcoroutines
      -g
      -fopenmp
      -Winterference-size
  )
  target_link_options(coros_benchmark PRIVATE -fopenmp)
endif()


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(coros_benchmark PRIVATE
      -std=c++23
      -stdlib=libc++
      -O3
      -g
      -fopenmp
  )
  target_link_options(coros_benchmark PRIVATE "-stdlib=libc++" "-lomp")
  #target_include_directories(coros_benchmark PRIVATE /usr/lib/llvm-14/include/c++/v1/)
  #target_link_directories(coros_benchmark PRIVATE /usr/lib/llvm-14/lib)
endif()
