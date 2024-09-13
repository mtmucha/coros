#include <gtest/gtest.h>

#include "start_tasks.h"
#include "thread_pool.h"
#include "enqueue_tasks.h"

TEST(NoWaitTaskTest, FireTask) {
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
  {
    coros::ThreadPool pool(1);

    EXPECT_EQ(coros::Task<void>::instance_count(), 0);
    coros::Task<void> t = []() -> coros::Task<void> {
      co_return;
    }();

    EXPECT_EQ(coros::Task<void>::instance_count(), 1);
    enqueue_tasks(pool,std::move(t));
    EXPECT_EQ(coros::Task<void>::instance_count(), 2);
  }
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
}


TEST(NoWaitTaskTest, NotExecutingTask) {
  {
    coros::ThreadPool tp(0);

    // No task created.
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 0 );

    coros::Task<std::string> t = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    coros::enqueue_tasks(tp, std::move(t));

    // Two tasks should exist one in the fire_task stored with contains
    // data from t and our current t in invalid state, since we moved it.
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 2);
  }
  // Task was not executed by the thread pool. Task t is destroyed when exiting scope.
  // Task inside threadpool should be destroyed in ThreadPool's destrcutor.
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(NoWaitTaskTest, NoThreadPoolParam) {
    coros::ThreadPool tp(2);

    std::atomic<bool> flag = false;

    coros::start_sync(tp, 
                      [](std::atomic<bool>& flag) -> coros::Task<void>{
                        coros::enqueue_tasks([](std::atomic<bool>& flag) -> coros::Task<void>{
                                               flag.store(true);
                                               co_return;
                                             }(flag));

                        co_return;
                      }(flag));
    
  
    while(!flag) {
      // busy waiting for the flag to be set
    }
    EXPECT_EQ(flag, true);
}

TEST(NoWaitTaskTest, MultipleTasks) {
  coros::ThreadPool tp{2};
  std::atomic<int> counter = 0;
  
  coros::Task<void> task = [](std::atomic<int>& counter) -> coros::Task<void> {
    auto t1 = [](std::atomic<int>& counter) -> coros::Task<void> {
      counter++;
      co_return;
    }(counter);
    auto t2 = [](std::atomic<int>& counter) -> coros::Task<void> {
      counter++;
      co_return;
    }(counter);

    coros::enqueue_tasks(std::move(t1), std::move(t2));

    co_return;
  }(counter);

  coros::start_sync(tp, task);
  
  // Busy wait
  while(counter != 2){}

  EXPECT_EQ(counter, 2);
}

TEST(NoWaitTaskTest, MultipleTasksThreadPool) {
  coros::ThreadPool tp2{2};
  coros::ThreadPool tp{2};
  std::atomic<int> counter = 0;
  
  coros::Task<void> task = [](std::atomic<int>& counter, coros::ThreadPool& tp) -> coros::Task<void> {
    auto t1 = [](std::atomic<int>& counter) -> coros::Task<void> {
      counter++;
      co_return;
    }(counter);
    auto t2 = [](std::atomic<int>& counter) -> coros::Task<void> {
      counter++;
      co_return;
    }(counter);

    coros::enqueue_tasks(tp, std::move(t1), std::move(t2));

    co_return;
  }(counter, tp2);

  coros::start_sync(tp, task);
  
  // Busy wait
  while(counter != 2){}

  EXPECT_EQ(counter, 2);
}

TEST(NoWaitTaskTest, Vector) {
  coros::ThreadPool tp{2};
  std::atomic<int> counter = 0;
  
  coros::Task<void> task = [](std::atomic<int>& counter) -> coros::Task<void> {

    std::vector<coros::Task<void>> vec;
    vec.push_back([](std::atomic<int>& counter) -> coros::Task<void> {
                    counter++;
                    co_return;
    }(counter));
    vec.push_back([](std::atomic<int>& counter) -> coros::Task<void> {
                    counter++;
                    co_return;
    }(counter));

    coros::enqueue_tasks(std::move(vec));

    co_return;
  }(counter);

  coros::start_sync(tp, task);
  
  // Busy wait
  while(counter != 2){}

  EXPECT_EQ(counter, 2);
}

TEST(NoWaitTaskTest, VectorThradpool) {
  coros::ThreadPool tp2{2};
  coros::ThreadPool tp{2};
  std::atomic<int> counter = 0;
  
  coros::Task<void> task = [](std::atomic<int>& counter, coros::ThreadPool& tp) -> coros::Task<void> {

    std::vector<coros::Task<void>> vec;
    vec.push_back([](std::atomic<int>& counter) -> coros::Task<void> {
                    counter++;
                    co_return;
    }(counter));
    vec.push_back([](std::atomic<int>& counter) -> coros::Task<void> {
                    counter++;
                    co_return;
    }(counter));

    coros::enqueue_tasks(tp, std::move(vec));

    co_return;
  }(counter, tp2);

  coros::start_sync(tp, task);
  
  // Busy wait
  while(counter != 2){}

  EXPECT_EQ(counter, 2);
}
