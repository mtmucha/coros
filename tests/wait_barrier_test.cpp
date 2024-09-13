#include <gtest/gtest.h>

#include "wait_barrier.h"

TEST(WaitBarrierTest, IntConstruction) {
    coros::detail::WaitBarrier barrier(5, nullptr);
    EXPECT_EQ(barrier.get_continuation(), nullptr);
}

TEST(WaitBarrierTest, DecreaseCounter) {
    coros::detail::WaitBarrier barrier(5, nullptr);
    barrier.decrement_and_resume();
    EXPECT_EQ(barrier.get_counter(), 4);
    EXPECT_EQ(barrier.get_continuation(), nullptr);
}


/*TEST(WaitBarrierTest, Async) {
{
  coros::ThreadPool tp{1};
  coros::WaitTask<coros::SharedBarrierAsync> t1 = 
      []() -> coros::WaitTask<coros::SharedBarrierAsync> {
        co_return; 
    }();

  EXPECT_EQ(coros::WaitTask<coros::SharedBarrierAsync>::instance_count(),
            1);

  coros::WaitForTasksAwaitableAsync<coros::ThreadPool, 1> awaitable{
    tp,
    std::array<coros::WaitTask<coros::SharedBarrierAsync>, 1>{std::move(t1)}};

  EXPECT_EQ(coros::WaitTask<coros::SharedBarrierAsync>::instance_count(),
            2);
}

}*/


