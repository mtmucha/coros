#include <gtest/gtest.h>
#include "thread_pool.h"

#include "start_tasks.h"

TEST(StartingTaskTest, BasicAssertions) {
    coros::StartTask t = []() -> coros::StartTask {
        co_return;
    }();
}

TEST(StartingTaskTest, BarrierSetReset) {
  coros::StartTask t = []() -> coros::StartTask { co_return; }();

  bool v = t.get_handle().promise().get_barrier().is_set();
  EXPECT_EQ(v, false);

  t.get_handle().resume();

  // task is done here
  v = t.get_handle().promise().get_barrier().is_set();
  EXPECT_EQ(v, true);
}

TEST(StartingTaskTest, SyncTask) {
  coros::ThreadPool tp{1};
  coros::Task<int> t = []() -> coros::Task<int> { co_return 42; }();

  coros::start_sync(tp, t);
  EXPECT_EQ(t.value(), 42);
}


TEST(StartingTaskTest, AsyncWait) {
  coros::ThreadPool tp(1);
  coros::Task<int> t = []() -> coros::Task<int> { co_return 42; }();
  
  auto bt = coros::start_async(tp, t);

  bt.wait();

  EXPECT_EQ(*t, 42);
}

// Test async_wait, when task2 completes before task1. 
TEST(StartingTaskTest, AsyncWaitTwoTasks) {

  for (int i = 0; i < 1000;i++) {
    coros::ThreadPool tp1(1);
    coros::ThreadPool tp2(1);

    std::atomic<bool> flag = false;

    coros::Task<int> t1 = [](std::atomic<bool>& flag) -> coros::Task<int> { 
        while(flag == false);
        co_return 43;
    }(flag);
    coros::Task<int> t2 = [](std::atomic<bool>& flag) -> coros::Task<int> { 
        flag = true;
        co_return 42; 
    }(flag);

    auto bt1 = coros::start_async(tp1, t1);
    auto bt2 = coros::start_async(tp2, t2);

    bt1.wait();
    bt2.wait();

    EXPECT_EQ(t1.value(), 43);
    EXPECT_EQ(t2.value(), 42);
  }
}

TEST(StartingTaskTest, AsyncAwaitMultipleTasks) {
  coros::ThreadPool tp(2);

  coros::Task<int> t1 = []() -> coros::Task<int> {co_return 42;}();
  coros::Task<int> t2 = []() -> coros::Task<int> {co_return 43;}();
  coros::Task<int> t3 = []() -> coros::Task<int> {co_return 44;}();

  auto bt1 = coros::start_async(tp, t1, t2, t3);

  bt1.wait();

  EXPECT_EQ(*t1, 42);
  EXPECT_EQ(*t2, 43);
  EXPECT_EQ(*t3, 44);
}

TEST(StartingTaskTest, AsyncAwaitLambda) {
  coros::ThreadPool tp(2);

  std::atomic<int> n1 = 0;
  std::atomic<int> n2 = 0;
  std::atomic<int> n3 = 0;

  auto bt = coros::start_async( 
    tp, 
    [](std::atomic<int>& n1) -> coros::Task<void> {n1 = 42; co_return;}(n1),
    [](std::atomic<int>& n2) -> coros::Task<void> {n2 = 43; co_return;}(n2),
    [](std::atomic<int>& n3) -> coros::Task<void> {n3 = 44; co_return;}(n3)
  );

  bt.wait();

  EXPECT_EQ(n1, 42);
  EXPECT_EQ(n2, 43);
  EXPECT_EQ(n3, 44);
}


TEST(StartingTaskTest, SyncWaitMultipleTasks) {
  coros::ThreadPool tp(2);

  coros::Task<int> t1 = []() -> coros::Task<int> {co_return 42;}();
  coros::Task<int> t2 = []() -> coros::Task<int> {co_return 43;}();
  coros::Task<int> t3 = []() -> coros::Task<int> {co_return 44;}();

  coros::start_sync(tp, t1, t2, t3);

  EXPECT_EQ(t1.value(), 42);
  EXPECT_EQ(t2.value(), 43);
  EXPECT_EQ(t3.value(), 44);
}

TEST(StartingTaskTest, SyncWaitLambda) {
  coros::ThreadPool tp(2);
  
  std::atomic<int> n1 = 0;
  std::atomic<int> n2 = 0;
  std::atomic<int> n3 = 0;

  coros::start_sync( 
    tp, 
    [](std::atomic<int>& n1) -> coros::Task<void> {n1 = 42; co_return;}(n1),
    [](std::atomic<int>& n2) -> coros::Task<void> {n2 = 43; co_return;}(n2),
    [](std::atomic<int>& n3) -> coros::Task<void> {n3 = 44; co_return;}(n3)
  );

  EXPECT_EQ(n1, 42);
  EXPECT_EQ(n2, 43);
  EXPECT_EQ(n3, 44);
}








