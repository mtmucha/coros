#include <gtest/gtest.h>
#include <type_traits> 

#include "chain_tasks.h"
#include "wait_tasks.h"
#include "start_tasks.h"


// Helper functions.
namespace {

coros::Task<int> add_one(int val) {
  co_return val + 1;
}

coros::Task<void> return_void([[maybe_unused]] int value) {
  co_return;
}

coros::Task<int> add_one_exception(int val) {
  throw std::bad_alloc();
  co_return val + 1;
}

coros::Task<void> accept_void() {
  co_return;
}

coros::Task<void> accept_void_throw() {
  throw std::bad_alloc();
  co_return;
}

coros::Task<double> return_double(int value) {
  co_return value;
}

coros::Task<int> fib(int index) {
  if (index == 0) co_return 0; 
  if (index == 1) co_return 1;

  coros::Task<int> a = fib(index - 1);
  coros::Task<int> b = fib(index - 2);

  co_await coros::wait_tasks(a, b);

  co_return *a + *b;
} 

} // anonymous namespace

TEST(ChainTest, SimpleChain) {
  std::expected<int, std::exception_ptr> ex{40};
  auto t = [](std::expected<int, std::exception_ptr>& ex) -> coros::Task<int> {
    auto awaitable = coros::chain_tasks(ex).and_then(add_one);
    auto res = co_await awaitable;
    co_return *res + 1;
  }(ex);
  t.get_handle().resume();
  EXPECT_EQ(*t, 42);
}

TEST(ChainTest, MultipleFunctions) {
  std::expected<int, std::exception_ptr> ex{38};
  auto t = [](std::expected<int, std::exception_ptr>& ex) -> coros::Task<int> {
    auto awaitable = coros::chain_tasks(ex).and_then(add_one)
                                              .and_then(add_one)
                                              .and_then(add_one);
    auto res = co_await awaitable;
    co_return *res + 1;
  }(ex);
  t.get_handle().resume();
  EXPECT_EQ(*t, 42);
}

TEST(ChainTest, ReturnVoid) {
  std::expected<int, std::exception_ptr> ex{38};
  auto t = [](std::expected<int, std::exception_ptr>& ex) -> coros::Task<void> {
    auto awaitable = coros::chain_tasks(ex).and_then(add_one)
                                              .and_then(add_one)
                                              .and_then(add_one)
                                              .and_then(return_void);
    auto res = co_await awaitable;
    co_return;
  }(ex);
  t.get_handle().resume();
}

TEST(ChainTest, ChainVoid) {
  std::expected<int, std::exception_ptr> ex{41};
  auto t = [](std::expected<int, std::exception_ptr>& ex) -> coros::Task<void> {
    auto awaitable = coros::chain_tasks(ex).and_then(add_one)
                                              .and_then(return_void)
                                              .and_then(accept_void)
                                              .and_then(accept_void)
                                              .and_then(accept_void);

    auto res = co_await awaitable;
    co_return;
  }(ex);
  t.get_handle().resume();
}

TEST(ChainTest, PropagateException) {
  std::expected<int, std::exception_ptr> ex{41};
  auto t = [](std::expected<int, std::exception_ptr>& ex) -> coros::Task<void> {
    auto awaitable = coros::chain_tasks(ex).and_then(add_one)
                                              .and_then(add_one_exception)
                                              .and_then(return_void);

    auto res = co_await awaitable;
    EXPECT_EQ(res.has_value(), false);
    EXPECT_THROW(std::rethrow_exception(res.error()), std::bad_alloc);
    co_return;
  }(ex);
  t.get_handle().resume();
}

TEST(ChainTest, TypeConversion) {
  std::expected<int, std::exception_ptr> ex{41};
  auto t = [](std::expected<int, std::exception_ptr>& ex) -> coros::Task<int> {
    auto awaitable = coros::chain_tasks(ex).and_then(add_one)
                                     .and_then(return_double);

    auto res = co_await awaitable;
    static_assert(
        std::is_same_v<std::expected<double, std::exception_ptr>,
                       std::remove_reference_t<decltype(res)>>);
    EXPECT_EQ(res.has_value(), true);
    EXPECT_EQ(*res, 42);
    static_assert(
        std::is_same_v<double, std::remove_reference_t<decltype(*res)>>, 
        "Type is not double : ");
  co_return *res;
  }(ex);
  t.get_handle().resume();
  EXPECT_EQ(*t, 42);
}

TEST(ChainTest, ChainTaskStart) {
  auto t = []() -> coros::Task<int> {
    auto t = []() -> coros::Task<int> {co_return 41;};
    auto awaitable = coros::chain_tasks(t()).and_then(add_one);

    auto res = co_await awaitable;
    EXPECT_EQ(res.has_value(), true);
    static_assert(
      std::is_same_v<std::remove_reference_t<decltype(*res)>,
                     int>,
      "NO"
    );
    co_return *res;
  }();
  t.get_handle().resume();
  EXPECT_EQ(*t, 42);
}

TEST(ChainTest, Fib) {
  coros::ThreadPool tp{4};
  auto t = []() -> coros::Task<int> {
    auto res = co_await coros::chain_tasks(fib(20)).and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(add_one);
    co_return *res;
  }();
  coros::start_sync(tp, t);
  EXPECT_EQ(*t, 6768);
}

TEST(ChainTest, FibVoid) {
  coros::ThreadPool tp{4};
  auto t = []() -> coros::Task<void> {
    auto res = co_await coros::chain_tasks(fib(20)).and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(return_void)
                                             .and_then(accept_void)
                                             .and_then(accept_void);
    EXPECT_TRUE(res.has_value());
    co_return;
  }();
  coros::start_sync(tp, t);
}

TEST(ChainTest, FibVoidThrow) {
  coros::ThreadPool tp{4};
  auto t = []() -> coros::Task<void> {
    auto res = co_await coros::chain_tasks(fib(20)).and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(return_void)
                                             .and_then(accept_void_throw)
                                             .and_then(accept_void);
    EXPECT_FALSE(res.has_value());
    EXPECT_THROW(std::rethrow_exception(res.error()), std::bad_alloc);
    co_return;
  }();
  coros::start_sync(tp, t);
}

TEST(ChainTest, Lambda) {
  coros::ThreadPool tp{4};
  auto t = []() -> coros::Task<void> {
    auto res = co_await coros::chain_tasks(fib(20)).and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(add_one)
                                             .and_then(return_void)
                                             .and_then(accept_void_throw)
                                             .and_then(accept_void);
    EXPECT_FALSE(res.has_value());
    EXPECT_THROW(std::rethrow_exception(res.error()), std::bad_alloc);
    co_return;
  }();
  coros::start_sync(tp, t);
}

class MoveOnlyInt {
 public:
  MoveOnlyInt(int value) : value_(value) {}
  MoveOnlyInt(MoveOnlyInt&& other) noexcept : value_(other.value_) {
    other.value_ = 0; // Optional: reset the source object
  }
  MoveOnlyInt& operator=(MoveOnlyInt&& other) noexcept {
    if (this != &other) {
      value_ = other.value_;
      other.value_ = 0; 
    }
    return *this;
  }
  MoveOnlyInt(const MoveOnlyInt&) = delete;
  MoveOnlyInt& operator=(const MoveOnlyInt&) = delete;
  int getValue() const { return value_; }
 private:
  int value_;
};

class CopyOnlyInt {
 public:
  CopyOnlyInt(int value) : value_(value) {}
  CopyOnlyInt(const CopyOnlyInt& other) : value_(other.value_) {}
  CopyOnlyInt& operator=(const CopyOnlyInt& other) {
    if (this != &other) { 
      value_ = other.value_;
    }
    return *this;
  }
  ~CopyOnlyInt(){};
  int getValue() const { return value_; }
 private:
  int value_;
};

coros::Task<MoveOnlyInt> add_one_move(MoveOnlyInt val) {
  co_return MoveOnlyInt(val.getValue() + 1);
}

coros::Task<CopyOnlyInt> add_one_copy(CopyOnlyInt val) {
  co_return CopyOnlyInt(val.getValue() + 1);
}


TEST(ChainTest, MoveOnlyInt) {
  auto t = []() -> coros::Task<MoveOnlyInt>  {
    std::expected<MoveOnlyInt, std::exception_ptr> ex{40};
    auto res = co_await coros::chain_tasks(std::move(ex)).and_then(add_one_move)
                                                   .and_then(add_one_move);
    co_return *res;
  }();
  t.get_handle().resume();
  EXPECT_EQ((*t).getValue(), 42);
}

TEST(ChainTest, CopyOnlyInt) {
  auto t = []() -> coros::Task<CopyOnlyInt>  {
    std::expected<CopyOnlyInt, std::exception_ptr> ex{40};
    auto res = co_await coros::chain_tasks(ex).and_then(add_one_copy)
                                        .and_then(add_one_copy);
    co_return *res;
  }();
  t.get_handle().resume();
  EXPECT_EQ((*t).getValue(), 42);
}

TEST(ChainTest, InitValue){
  coros::ThreadPool tp{4};
  auto t = []() -> coros::Task<int> {
    auto res = co_await coros::chain_tasks(40).and_then(add_one)
                                        .and_then(add_one);
    EXPECT_TRUE(res.has_value());
    co_return *res;
  }();
  coros::start_sync(tp, t);
  EXPECT_EQ(*t, 42);
}





























