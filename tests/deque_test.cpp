#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <coroutine>

#include "deque.h"
#include "task_life_time.h"
#include "enqueue_tasks.h"


TEST(DequeTest, PushAndPop) {
  
  std::unique_ptr<coros::detail::Dequeue> q = std::make_unique<coros::detail::Dequeue>();
  //coros::detail::Dequeue q;
  q->pushBottom({std::noop_coroutine(), coros::detail::TaskLifeTime::NOOP});

  EXPECT_TRUE(q->popBottom().has_value());
  EXPECT_FALSE(q->popBottom().has_value());
}

TEST(DequeTest, Steal) {
  std::unique_ptr<coros::detail::Dequeue> q = std::make_unique<coros::detail::Dequeue>();
  q->pushBottom({std::noop_coroutine(), coros::detail::TaskLifeTime::NOOP});

  EXPECT_TRUE(q->steal().has_value());
  EXPECT_FALSE(q->steal().has_value());
  EXPECT_FALSE(q->popBottom().has_value());
}

TEST(DequeTest, ConcurrentPushSteal) {
  std::unique_ptr<coros::detail::Dequeue> q = std::make_unique<coros::detail::Dequeue>();

  std::thread producer([&](){
    for (int i = 0; i < 1'000; i++) {
      q->pushBottom({std::noop_coroutine(), coros::detail::TaskLifeTime::NOOP});
    }
  });

  std::thread consumer([&](){
    int count = 0;
    while(count != 1'000) {
      if (q->steal().has_value()) {
        count++;
      }
    }
  });

  producer.join();
  consumer.join();
}

TEST(DequeTest, BufferGrowTest) {
  std::unique_ptr<coros::detail::Dequeue> q = std::make_unique<coros::detail::Dequeue>(2,1);
  EXPECT_EQ(q->get_buffer_size(), 2);
  q->pushBottom({std::noop_coroutine(), coros::detail::TaskLifeTime::NOOP});
  q->pushBottom({std::noop_coroutine(), coros::detail::TaskLifeTime::NOOP});
  q->pushBottom({std::noop_coroutine(), coros::detail::TaskLifeTime::NOOP});

  // we double in size
  EXPECT_EQ(q->get_buffer_size(), 8);

  EXPECT_TRUE(q->popBottom().has_value());
  EXPECT_TRUE(q->popBottom().has_value());
  EXPECT_TRUE(q->popBottom().has_value());
  EXPECT_FALSE(q->popBottom().has_value());

  EXPECT_EQ(q->get_buffer_size(), 8);
}

TEST(DequeTest, DestructionNoWaitTask) {
  {
    EXPECT_EQ(coros::Task<int>::instance_count(), 0);
    EXPECT_EQ(coros::NoWaitTaskPromise::instance_count(), 0);
    std::unique_ptr<coros::detail::Dequeue> q = std::make_unique<coros::detail::Dequeue>(2,1);
    
    auto t1 = []() -> coros::NoWaitTask{ co_return; }();
    auto t2 = []() -> coros::NoWaitTask{ co_return; }();
    auto t3 = []() -> coros::NoWaitTask{ co_return; }();
    auto t4 = []() -> coros::NoWaitTask{ co_return; }();

    EXPECT_EQ(q->get_buffer_size(), 2);
    EXPECT_EQ(coros::NoWaitTaskPromise::instance_count(), 4);

    q->pushBottom({t1.get_handle(), coros::detail::TaskLifeTime::THREAD_POOL_MANAGED});
    q->pushBottom({t2.get_handle(), coros::detail::TaskLifeTime::THREAD_POOL_MANAGED});
    q->pushBottom({t3.get_handle(), coros::detail::TaskLifeTime::THREAD_POOL_MANAGED});
    q->pushBottom({t4.get_handle(), coros::detail::TaskLifeTime::THREAD_POOL_MANAGED});


    EXPECT_EQ(q->get_buffer_size(), 8);
    EXPECT_EQ(coros::NoWaitTaskPromise::instance_count(), 4);
  }
  EXPECT_EQ(coros::NoWaitTaskPromise::instance_count(), 0);
}

TEST(DequeTest, DestructionTask) {
  EXPECT_EQ(coros::detail::SimplePromise<int>::instance_count(), 0);
  {
    auto t1 = []() -> coros::Task<int>{ co_return 42; }();
    auto t2 = []() -> coros::Task<int>{ co_return 42; }();
    auto t3 = []() -> coros::Task<int>{ co_return 42; }();
    auto t4 = []() -> coros::Task<int>{ co_return 42; }();
    {
    std::unique_ptr<coros::detail::Dequeue> q = std::make_unique<coros::detail::Dequeue>(2,1);

      EXPECT_EQ(q->get_buffer_size(), 2);
      EXPECT_EQ(coros::detail::SimplePromise<int>::instance_count(), 4);

      q->pushBottom({t1.get_handle(), coros::detail::TaskLifeTime::SCOPE_MANAGED});
      q->pushBottom({t2.get_handle(), coros::detail::TaskLifeTime::SCOPE_MANAGED});
      q->pushBottom({t3.get_handle(), coros::detail::TaskLifeTime::SCOPE_MANAGED});
      q->pushBottom({t4.get_handle(), coros::detail::TaskLifeTime::SCOPE_MANAGED});

      EXPECT_EQ(q->get_buffer_size(), 8);
    }
    EXPECT_EQ(coros::detail::SimplePromise<int>::instance_count(), 4);
  }
  EXPECT_EQ(coros::detail::SimplePromise<int>::instance_count(), 0);
}

