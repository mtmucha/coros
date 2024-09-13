cmake_minimum_required(VERSION 3.20)

enable_testing()

project(coros)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Exclude pthread search for MSVC
if (NOT MSVC)
    find_package(Threads REQUIRED)
endif()


# compiler options for GCC
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-std=c++23 -Wall -Wextra -Wpedantic -Wno-interference-size -fcoroutines) 
endif()

# compiler options for Clang
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-std=c++23 -stdlib=libc++ -Wall -Wextra -Wpedantic) 
    add_link_options(-std=c++23, -stdlib=libc++)
endif()

# Compiler options for MSVC
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/std:c++latest /W4 /await:strict /permissive-)
    
    # add_compile_options(/W4)  # /W4 is similar to -Wall -Wextra in GCC/Clang

    #add_compile_options(/wd4996)
endif()

add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(examples)


