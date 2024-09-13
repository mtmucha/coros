#include <gtest/gtest.h>
#include <coroutine>
#include <typeinfo>
#include "constructor_counter.hpp"

#include "task.h"

TEST(TaskTest, TaskTrivialType) {
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
  {
    coros::Task<int> t  = []() -> coros::Task<int> {
      co_return 42;
    }();
    EXPECT_EQ(coros::Task<int>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_EQ(t.value(), 42);
  } 
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
}

TEST(TaskTest, TaskVoid) {
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
  {
    coros::Task<void> t  = []() -> coros::Task<void> {
      co_return;
    }();
    EXPECT_EQ(coros::Task<void>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_NO_THROW(t.value());
  } 
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
}

TEST(TaskTest, TaskAccessResultBeforeSet) {
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
  {
    coros::Task<int> t  = []() -> coros::Task<int> {
      co_return 42;
    }();
    EXPECT_EQ(coros::Task<int>::instance_count(), 1);
    EXPECT_THROW(t.value(), std::bad_expected_access<void>);
  } 
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
}

TEST(TaskTest, TaskException) {
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
  {
    coros::Task<int> t  = []() -> coros::Task<int> {
      throw std::bad_alloc();
      co_return 42;
    }();
    EXPECT_EQ(coros::Task<int>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_THROW(std::rethrow_exception(t.error()), std::bad_alloc);
  } 
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
}

// This test also covers no_throw_constructible type for return_value().
TEST(TaskTest, TaskNonTrivialType) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_EQ(t.value(), "Hello");
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, TaskNonTrivialTypeNotInitialized) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, MoveConstructor) {
  {
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
    coros::Task<std::string> t1  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);

    std::coroutine_handle<> original_handle = t1.get_handle();
    coros::Task<std::string> t2 = std::move(t1);

    // A new task is constructed, now two instances of the task live.
    // One should have nullptr handle (moved from task, t1) and t2 should 
    // have the original handle.
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 2); EXPECT_EQ(t1.get_handle(), nullptr);
    EXPECT_EQ(t2.get_handle(), original_handle);

    t2.get_handle().resume();
    std::string result = t2.value();
    EXPECT_EQ(result, "Hello");
  }
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, ControlTransfer) {
  {
    EXPECT_EQ(coros::Task<int>::instance_count(), 0);
    coros::Task<int> t1  = []() -> coros::Task<int> {
      co_return 42;
    }();
    coros::Task<int> t2  = [&](coros::Task<int> t) -> coros::Task<int> {
      co_await t;
      co_return t.value() + 1;
    }(std::move(t1));
    // 3 instances live, t1, t2, and t in the t2.
    EXPECT_EQ(coros::Task<int>::instance_count(), 3);
    EXPECT_EQ(t1.get_handle(), nullptr);

    t2.get_handle().resume();
    int result = t2.value();
    EXPECT_EQ(result, 43); 
    EXPECT_EQ(coros::Task<int>::instance_count(), 3);
  }
  EXPECT_EQ(coros::Task<int>::instance_count(), 0);
}

TEST(TaskTest, OperatorAsterisk) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_EQ(*t, "Hello");
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, OperatorAsteriskRvalue) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    t.get_handle().resume();
    std::string result = *std::move(t);
    EXPECT_EQ(result, "Hello");
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, OperatorAsteriskRvalueCounter) {
  coros::test::ConstructorCounter::clear_count();
  coros::Task<coros::test::ConstructorCounter> t  = []() -> coros::Task<coros::test::ConstructorCounter> {
    co_return coros::test::ConstructorCounter();
  }();
  t.get_handle().resume();
  EXPECT_EQ(coros::test::ConstructorCounter::move_constructed, 1);
  [[maybe_unused]] coros::test::ConstructorCounter result = *std::move(t);
  EXPECT_EQ(coros::test::ConstructorCounter::move_constructed, 2);
}

TEST(TaskTest, OperatorAsteriskVoid) {
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
  {
    coros::Task<void> t  = []() -> coros::Task<void> {
      co_return;
    }();
    EXPECT_EQ(coros::Task<void>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_NO_THROW(*t);
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

// TODO : Need to implement operator->().

TEST(TaskTest, ValueOrSet) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_EQ(t.value_or("Bye"), "Hello");
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, ValueOrNotSet) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    EXPECT_EQ(t.value_or("Bye"), "Bye");
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, ValueOrMove) {
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
  {
    coros::Task<std::string> t  = []() -> coros::Task<std::string> {
      co_return "Hello";
    }();
    EXPECT_EQ(coros::Task<std::string>::instance_count(), 1);
    t.get_handle().resume();
    std::string result = std::move(t).value_or("Bye");
    EXPECT_EQ(result, "Hello");
  } 
  EXPECT_EQ(coros::Task<std::string>::instance_count(), 0);
}

TEST(TaskTest, ValueOrMoveCounter) {
  coros::test::ConstructorCounter::clear_count();
  EXPECT_EQ(coros::Task<coros::test::ConstructorCounter>::instance_count(), 0);
  {
    coros::Task<coros::test::ConstructorCounter> t  = []() -> coros::Task<coros::test::ConstructorCounter> {
      co_return coros::test::ConstructorCounter();
    }();
    EXPECT_EQ(coros::Task<coros::test::ConstructorCounter>::instance_count(), 1);
    t.get_handle().resume();
    EXPECT_EQ(coros::test::ConstructorCounter::move_constructed, 1);
    [[maybe_unused]] coros::test::ConstructorCounter result = 
      std::move(t).value_or(coros::test::ConstructorCounter());
    // Check whether instnace of the ConstructorCounter was constructed
    // in the value_or.
    EXPECT_EQ(coros::test::ConstructorCounter::move_constructed, 2);
  } 
  EXPECT_EQ(coros::Task<coros::test::ConstructorCounter>::instance_count(), 0);
}

TEST(TaskTest, OperatorBoolSet) {
  coros::Task<std::string> t  = []() -> coros::Task<std::string> {
    co_return "Hello";
  }();
  t.get_handle().resume();

  EXPECT_TRUE(t);
}
TEST(TaskTest, OperatorBoolNotSet) {
  coros::Task<std::string> t  = []() -> coros::Task<std::string> {
    co_return "Hello";
  }();

  EXPECT_FALSE(t);
}

TEST(TaskTest, HasValueSet) {
  coros::Task<std::string> t  = []() -> coros::Task<std::string> {
    co_return "Hello";
  }();
  t.get_handle().resume();

  EXPECT_TRUE(t.has_value());
}

TEST(TaskTest, HasValueNotSet) {
  coros::Task<std::string> t  = []() -> coros::Task<std::string> {
    co_return "Hello";
  }();

  EXPECT_FALSE(t.has_value());
}

TEST(TaskTest, VoidSetValue) {
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
  {
    auto t = []() -> coros::Task<void>{
      co_return;
    }();
    EXPECT_EQ(t.has_value(), false);
    t.get_handle().resume();
    EXPECT_EQ(t.has_value(), true);
  }
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
}

TEST(TaskTest, VoidGetValue) {
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
  {
    auto t = []() -> coros::Task<void>{
      co_return;
    }();
    t.get_handle().resume();
    EXPECT_EQ(t.has_value(), true);
    EXPECT_NO_THROW(*t);
  }
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
}

TEST(TaskTest, VoidThrowException) {
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
  {
    auto t = []() -> coros::Task<void>{
      throw std::bad_typeid();
      co_return;
    }();
    t.get_handle().resume();
    EXPECT_EQ(t.has_value(), false);
    EXPECT_THROW(std::rethrow_exception(t.error()), std::bad_typeid);
  }
  EXPECT_EQ(coros::Task<void>::instance_count(), 0);
}

namespace {

struct ListInitialized {
  ListInitialized(std::initializer_list<int> list) {
    std::size_t i = 0;
    for (auto elem : list) {
      arr_[i++] = elem;
    }
  };

  std::array<int, 2> arr_;
};

struct ListInitializedNoExcept {
  ListInitializedNoExcept(std::initializer_list<int> list) noexcept {
    std::size_t i = 0;
    for (auto elem : list) {
      arr_[i++] = elem;
    }
  };

  std::array<int, 2> arr_;
};

}

TEST(TaskTest, InitializerList) {
  EXPECT_EQ(coros::Task<ListInitialized>::instance_count(), 0);
  {
    auto t = []() -> coros::Task<ListInitialized>{
      co_return {41, 42};
    }();
    t.get_handle().resume();
    EXPECT_EQ(t.has_value(), true);
    EXPECT_EQ(t.value().arr_[0], 41);
    EXPECT_EQ(t.value().arr_[1], 42);
  }
  EXPECT_EQ(coros::Task<ListInitialized>::instance_count(), 0);
}

TEST(TaskTest, InitializerListNoThrow) {
  EXPECT_EQ(coros::Task<ListInitialized>::instance_count(), 0);
  {
    auto t = []() -> coros::Task<ListInitializedNoExcept>{
      co_return {41, 42};
    }();
    t.get_handle().resume();
    EXPECT_EQ(t.has_value(), true);
    EXPECT_EQ(t.value().arr_[0], 41);
    EXPECT_EQ(t.value().arr_[1], 42);
  }
  EXPECT_EQ(coros::Task<ListInitialized>::instance_count(), 0);
}

