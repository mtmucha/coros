#include "thread_pool.h"
#include <gtest/gtest.h>

#include "wait_tasks.h"
#include "start_tasks.h"

namespace {
coros::Task<int> fib(coros::ThreadPool& tp, int index) {
  if (index == 0) co_return 0; 
  if (index == 1) co_return 1;

  coros::Task<int> a = fib(tp, index - 1);
  coros::Task<int> b = fib(tp, index - 2);

  co_await coros::wait_tasks(a, b);

  co_return *a + *b;
}

}

TEST(ThreadPoolTest, Construction) {
  coros::ThreadPool tp{2};
}


TEST(ThreadPoolTest, Fib) {
  coros::ThreadPool tp{4};
  coros::Task<int> t = fib(tp, 20);
  coros::start_sync(tp, t);
  EXPECT_EQ(*t, 6765);
}
