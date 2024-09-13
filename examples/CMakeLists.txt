cmake_minimum_required(VERSION 3.20)

project(coros_example)

add_executable(simple_task_execution simple_task_execution.cpp)
add_executable(waiting waiting.cpp)
add_executable(enqueueing enqueueing.cpp)
add_executable(chaining chaining.cpp)
add_executable(fib fib.cpp)

target_include_directories(simple_task_execution PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(waiting PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(enqueueing PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(chaining PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(fib PUBLIC ${CMAKE_SOURCE_DIR}/include)



