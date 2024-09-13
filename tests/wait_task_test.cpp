#include <gtest/gtest.h>
#include <thread>

#include "wait_tasks.h"
#include "start_tasks.h"
#include "thread_pool.h"


TEST(WaitTaskTest, SettingPromiseBarrier) {
  coros::detail::WaitTaskPromise<coros::detail::WaitBarrier> promise = 
  coros::detail::WaitTaskPromise<coros::detail::WaitBarrier>{};
  coros::detail::WaitBarrier bar{10, nullptr};
  promise.set_barrier(&bar);
  EXPECT_EQ(promise.get_barrier().get_counter(), 10);
}

TEST(WaitTaskTest, MoveConstructor) {
  EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
  {
    coros::detail::WaitTask<coros::detail::WaitBarrier> task = []() -> coros::detail::WaitTask<coros::detail::WaitBarrier> {
        co_return;
    }();
    EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 1);

    std::coroutine_handle<> old_handle = task.get_handle();
    coros::detail::WaitTask<coros::detail::WaitBarrier> task2 = std::move(task);
    EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 2);

    EXPECT_EQ(task.get_handle(), nullptr);
    EXPECT_EQ(task2.get_handle(), old_handle);
  }
    EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
}

TEST(WaitTaskTest, WaitTaskDestructor) {
  EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
  {
    coros::Task<int> t = []() -> coros::Task<int> {
        co_return 42;
    }();

    coros::detail::WaitTask<coros::detail::WaitBarrier> wt = coros::detail::create_wait_task<coros::detail::WaitBarrier>(t);
    EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 1);
    EXPECT_EQ(coros::Task<int>::instance_count(), 1);
  }
  EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
}


TEST(WaitTaskTest, Construction) {
  EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
  {
    coros::detail::WaitTask<coros::detail::WaitBarrier> t1 = []() -> coros::detail::WaitTask<coros::detail::WaitBarrier> {
        co_return;
    }();

    coros::detail::WaitTask<coros::detail::WaitBarrier> t2 = []() -> coros::detail::WaitTask<coros::detail::WaitBarrier> {
        co_return;
    }();
    EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 2);

    std::coroutine_handle<> old_t1_h = t1.get_handle();
    std::coroutine_handle<> old_t2_h = t2.get_handle();

    EXPECT_NE(t1.get_handle(), nullptr);
    EXPECT_NE(t2.get_handle(), nullptr);

    coros::ThreadPool tp{1};

    coros::WaitTasksAwaitable awaiter = 
      coros::WaitTasksAwaitable<std::array<coros::detail::WaitTask<coros::detail::WaitBarrier>, 2>>{
        tp, 
        {std::move(t1), std::move(t2)},
        {2,nullptr}};

    std::array<coros::detail::WaitTask<coros::detail::WaitBarrier>, 2>& moved_tasks = awaiter.get_tasks();

    EXPECT_EQ(moved_tasks.size(), 2);
    // moved tasks are equal to the old one created
    EXPECT_EQ(moved_tasks[0].get_handle(), old_t1_h);
    EXPECT_EQ(moved_tasks[1].get_handle(), old_t2_h);
    // tasks are not suspended at final suspension point
    EXPECT_EQ(moved_tasks[0].get_handle().done(), false);
    EXPECT_EQ(moved_tasks[1].get_handle().done(), false);
  }
  EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
}


TEST(WaitTaskTest, WaitForTasks) {
  coros::ThreadPool tp{1};

  coros::Task<int> t = [&]() -> coros::Task<int> {
    coros::Task<int> a = [](int num) -> coros::Task<int> {
        co_return num;
    }(1);
    coros::Task<int> b = [](int num) -> coros::Task<int> {
        co_return num;
    }(2);

    co_await coros::wait_tasks(a, b);

    co_return *a + *b;
  }();

  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 3);
}

TEST(WaitTaskTest, WaitForTasksVector) {
  coros::ThreadPool tp{2};

  coros::Task<int> t = [&]() -> coros::Task<int> {
    std::vector<coros::Task<int>> vec;
    for (size_t i = 1; i <= 10;i++) {
      vec.push_back(
        [](int num) -> coros::Task<int> {
            co_return num;
        }(i)
      );
    }

    co_await coros::wait_tasks(vec);

    int result_sum = 0;
    for (auto& task : vec) {
      result_sum += *task;
    }

    co_return result_sum;
  }();

  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 55);
}

coros::Task<int> fib2(int index) {
  if (index == 0) co_return 0; 
  if (index == 1) co_return 1;

  coros::Task<int> a = fib2(index - 1);
  coros::Task<int> b = fib2(index - 2);

  co_await coros::wait_tasks(a, b);

  co_return *a + *b;
}

TEST(WaitTaskTest, WaitTaskFibonacci) {
  coros::ThreadPool tp{8};
  coros::Task<int> t = fib2(20);
  coros::start_sync(tp, t);
  EXPECT_EQ(t.value(), 6765);
}

coros::Task<int> moveTaskExecution(coros::ThreadPool& tp, int index) {
  auto t1 = fib2(index);
  auto b =  coros::wait_tasks(tp, t1);

  co_await b;

  co_return *t1;
}

// TODO : Could check the moving of the execution through thread ids.
TEST(WaitTaskTest, WaitTaskFibonacciDifferentPool) {
  coros::ThreadPool tp1{1};
  coros::ThreadPool tp2{1};

  coros::Task<int> t = moveTaskExecution(tp2, 20);
  coros::start_sync(tp1, t);
  EXPECT_EQ(t.value(), 6765);
}


TEST(WaitTaskTest, Lambda) {
  coros::ThreadPool tp1{2};

  std::atomic<int> n = 0;

  coros::start_sync(
    tp1, 
    [&]() -> coros::Task<void>{ 
      auto t = fib2(20);
      co_await t;
      n = *t;
    }()
  );

  EXPECT_EQ(n, 6765);
}

TEST(WaitTaskTest, RValueWaitTask) {
  auto t1 = []() -> coros::Task<void>{
      co_return;
    }();
  auto wt = coros::detail::create_wait_task<coros::detail::WaitBarrier>(std::move(t1));
}

TEST(WaitTaskTest, DifferentTaskTypes) {
  EXPECT_EQ(coros::detail::WaitTask<coros::detail::WaitBarrier>::instance_count(), 0);
  {
    coros::Task<int> a = [](int num) -> coros::Task<int> {
        co_return num;
    }(2);
    
    coros::Task<void> b = []() -> coros::Task<void> {
        co_return;
    }();

    coros::Task<int> c = [](coros::Task<int>& a, coros::Task<void>& b) -> coros::Task<int> {
      co_await coros::wait_tasks(a, b);
      co_return 42;
    }(a, b);
    EXPECT_EQ(coros::Task<int>::instance_count(), 2);
    EXPECT_EQ(coros::Task<void>::instance_count(), 1);

    coros::ThreadPool tp(1);
    coros::start_sync(tp, c);
    EXPECT_EQ(*c, 42);
  }
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
}

TEST(WaitTaskTest, Async) {
  coros::ThreadPool tp{1};

  coros::Task<int> t = [&]() -> coros::Task<int> {
    coros::Task<int> a = [](int num) -> coros::Task<int> {
        co_return num;
    }(1);
    coros::Task<int> b = [](int num) -> coros::Task<int> {
        co_return num;
    }(2);

    co_await coros::wait_tasks_async(a, b);

    co_return *a + *b;
  }();

  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 3);
}

TEST(WaitTaskTest, AsyncAwaitable) {
  coros::ThreadPool tp{1};

  coros::Task<int> t = [&]() -> coros::Task<int> {
    coros::Task<int> a = [](int num) -> coros::Task<int> {
        co_return num;
    }(1);
    coros::Task<int> b = [](int num) -> coros::Task<int> {
        co_return num;
    }(2);

    auto awaitable = coros::wait_tasks_async(a, b);

    co_await awaitable;

    co_return *a + *b;
  }();

  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 3);
}


thread_local std::mt19937 gen(std::random_device{}());

int generate_random_numbers() {
  std::uniform_int_distribution<> distrib(1, 20);
  return distrib(gen);
}


coros::Task<int> fib_async(int index) {
  if (index == 0) co_return 0; 
  if (index == 1) co_return 1;

  coros::Task<int> a = fib_async(index - 1);
  coros::Task<int> b = fib_async(index - 2);

  auto awaitable = coros::wait_tasks_async(a, b);
  // std::chrono::milliseconds duration(generate_random_numbers());
  // std::this_thread::sleep_for(duration);
  co_await awaitable;

  co_return *a + *b;
}

TEST(WaitTaskTest, WaitTaskFibonacciAsync) {
  coros::ThreadPool tp{8};
  coros::Task<int> t = fib_async(20);
  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 6765);
}


coros::Task<void> foo() {
  auto t1 = []() -> coros::Task<void> {
    std::chrono::milliseconds duration(generate_random_numbers());
    std::this_thread::sleep_for(duration);
    co_return;
  }();

  auto t2 = []() -> coros::Task<void> {
    std::chrono::milliseconds duration(generate_random_numbers());
    std::this_thread::sleep_for(duration);
    co_return;
  }();

  auto awaitable = coros::wait_tasks_async(t1, t2);
  std::chrono::milliseconds duration(generate_random_numbers());
  std::this_thread::sleep_for(duration);
  co_await awaitable;

  co_return;
}

// Mainly for the sanitizers.
TEST(WaitTaskTest, WaitTaskAsyncRun) {
  coros::ThreadPool tp{4};

  for (size_t i = 0; i < 1000; i++) {
    coros::Task<void> t = foo();
    coros::start_sync(tp, t);
  }
}

TEST(WaitTaskTest, WaitTaskPoolVector) {
  coros::ThreadPool tp{2};
  coros::ThreadPool tp2{2};

  coros::Task<int> t = [](coros::ThreadPool& tp) -> coros::Task<int> {
    std::vector<coros::Task<int>> vec;
    for (size_t i = 1; i <= 10;i++) {
      vec.push_back(
        [](int num) -> coros::Task<int> {
            co_return num;
        }(i)
      );
    }

    co_await coros::wait_tasks(tp, vec);

    int result_sum = 0;
    for (auto& task : vec) {
      result_sum += *task;
    }

    co_return result_sum;
  }(tp2);

  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 55);
}


TEST(WaitTaskTest, WaitTaskAsyncVector) {

  coros::ThreadPool tp{2};

  coros::Task<int> t = []() -> coros::Task<int> {
    std::vector<coros::Task<int>> vec;
    for (size_t i = 1; i <= 10;i++) {
      vec.push_back(
        [](int num) -> coros::Task<int> {
            co_return num;
        }(i)
      );
    }

    auto awaitable = coros::wait_tasks_async(vec);

    co_await awaitable;

    int result_sum = 0;
    for (auto& task : vec) {
      result_sum += *task;
    }

    co_return result_sum;
  }();

  coros::start_sync(tp, t);

  EXPECT_EQ(t.value(), 55);
}








