project(coros_test)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the Clang include directory variable
# set(CLANG_INCLUDE_DIR /usr/lib/clang/18/include)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.12.0
)

FetchContent_MakeAvailable(googletest)

# add test files 
add_executable (
  coros_test 
  task_test.cpp
  thread_pool_test.cpp
  start_tasks_test.cpp
  wait_barrier_test.cpp
  wait_task_test.cpp
  deque_test.cpp
  enqueue_tasks_test.cpp
  chain_test.cpp
)

target_include_directories(coros_test 
  PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(coros_test gtest_main)

# Macro to enable additional testing functionalities and tests in code.
# target_compile_definitions(coros_test PRIVATE COROS_TEST_ COROS_TEST_DEQUE_)
# target_compile_definitions(coros_test PRIVATE COROS_TEST_ )

# Set default sanitizer to address if not provided
if (NOT DEFINED DEQUE)
  set(DEQUE "NO")
endif()

# Whether to use testing deque.
if (DEQUE STREQUAL "TEST")
  target_compile_definitions(coros_test PRIVATE COROS_TEST_ COROS_TEST_DEQUE_)
else()
  # This needs to be set for the tests to work.
  target_compile_definitions(coros_test PRIVATE COROS_TEST_ )
endif()

# Set default sanitizer to address if not provided
if (NOT DEFINED SANITIZER)
  set(SANITIZER "address")
endif()

if (SANITIZER STREQUAL "thread") 
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(coros_test PRIVATE -fsanitize=thread -Wno-interference-size -g)
    target_link_options(coros_test PRIVATE -fsanitize=thread -g)
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    #target_include_directories(coros_test PRIVATE CLANG_INCLUDE_DIR)
    target_compile_options(coros_test PRIVATE 
      -stdlib=libc++
      -fsanitize=thread 
      -g
    )
    target_link_options(coros_test PRIVATE 
      -stdlib=libc++ 
      -fsanitize=thread 
      -g
    )
  endif()
elseif (SANITIZER STREQUAL "address")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(coros_test PRIVATE -fsanitize=address,leak,undefined -Wno-interference-size -g)
    target_link_options(coros_test PRIVATE -fsanitize=address,leak,undefined -g)
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    #target_include_directories(coros_test PRIVATE CLANG_INCLUDE_DIR)
    target_compile_options(coros_test PRIVATE 
      -stdlib=libc++
      -fsanitize=address,leak,undefined
      -g
    )
    target_link_options(coros_test PRIVATE 
      -stdlib=libc++ 
      -fsanitize=address,leak,undefined
      -Winterference-size
      -g
    )
  endif()
else()
    message(WARNING "Unknown sanitizer: ${SANITIZER}. No sanitizer will be applied.")
endif()

include(GoogleTest)
gtest_discover_tests(coros_test)
