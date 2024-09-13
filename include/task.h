#ifndef COROS_INCLUDE_TASK_H_
#define COROS_INCLUDE_TASK_H_

#include <coroutine>
#include <exception>
#include <expected>
#include <type_traits>

#include "constructor_counter.hpp"

namespace coros {


// C++ coroutines cannot accept parameters which do not
// satify this concept. Coroutine parameter is passed as an xvalue
// into a coroutine state to be copied/moved. Therefore, coros::Task<T> only
// works with such types.
template<typename T>
concept TaskRetunType = std::is_constructible_v<T,T>
                        || std::is_void_v<T>;

// Forward declaration for Task, so it can be used in the Promise object.
template <TaskRetunType ReturnValue>
class Task;

namespace detail {

// This awaiter resumes a coroutine by passing it a handle.
// 
// task<T> foo() {
//   co_await BasicAwaiter{bar_handle};
// }
//
// Pauses foo and resumes bar.
class ResumeAwaiter {
 public:
  constexpr bool await_ready() noexcept { return false; }

  std::coroutine_handle<> await_suspend  (
      [[maybe_unused]] std::coroutine_handle<> currently_suspended) noexcept {
    return to_resume_;
  }

  void await_resume() noexcept {}

  std::coroutine_handle<> to_resume_;
};

// Needs to be defined when running tests. Allows for checking the number
// of task instances alive.
#ifdef COROS_TEST_
  #define INSTANCE_COUNTER_SIMPLE_PROMISE_ : private coros::test::InstanceCounter<SimplePromise<ReturnValue>>
#else
  #define INSTANCE_COUNTER_SIMPLE_PROMISE_
#endif

// Promise object assosiacted with Task. 
template <TaskRetunType ReturnValue>
class SimplePromise  INSTANCE_COUNTER_SIMPLE_PROMISE_ {
 public:

  using ResultType = std::expected<ReturnValue, std::exception_ptr>;

  template<typename T = SimplePromise<ReturnValue>>
  static std::enable_if_t<std::is_base_of_v<coros::test::InstanceCounter<SimplePromise<ReturnValue>>, T>, std::size_t>
  instance_count() {
    return coros::test::InstanceCounter<SimplePromise<ReturnValue>>::instance_count();
  }

  // Constructs a Task object that is returned to the user, when creating a task. 
  // Coroutine handle (pointing to the coroutine state) is stored in Task object. This
  // allows for manipulating coroutine state and promise object through task.
  //
  // Task<int> foo() {co_return 42;} 
  //
  // Task<int> object is constructed and returned to user.
  Task<ReturnValue> get_return_object() noexcept {
    return Task<ReturnValue>{std::coroutine_handle<SimplePromise>::from_promise(*this)};
  };

  // Lazily evaluated coroutine, suspend on initial_suspend. 
  std::suspend_always initial_suspend() noexcept { return {}; }
  
  // If any task awaits thsi task, it is resumed. Otherwise default
  // value of noop_coroutine is passed to ResumeAwaiter, returning
  // control back to caller.
  ResumeAwaiter final_suspend() noexcept {
    return ResumeAwaiter{continuation_};
  }
  
  template <typename T = ReturnValue, typename U>
  requires (std::is_nothrow_constructible_v<T, std::remove_reference_t<U>>
            || std::is_nothrow_constructible_v<T, U>)
  void return_value(U&& val) noexcept {
    result_.emplace(std::move(val));
  }

  // Construct temporary object that can be moved/copied inot the result_;
  // Used in case the return type can throw in construction. Temporary is 
  // created and moved/copied into return value.
  template<typename T = ReturnValue, typename U>
  requires (!std::is_nothrow_constructible_v<T, std::remove_reference_t<U>>
            && (std::is_constructible_v<T, std::remove_reference_t<U>>
                || std::is_constructible_v<T, U>))
  void return_value(U&& val) {
    result_ = T{std::move(val)};
  }

  template<typename T = ReturnValue, typename U>
  requires (std::is_nothrow_constructible_v<T, std::initializer_list<U>>)
  void return_value(std::initializer_list<U> list) {
    result_.emplace(list);
  }

  template<typename T = ReturnValue, typename U>
  requires (!std::is_nothrow_constructible_v<T, std::initializer_list<U>>
            && std::is_constructible_v<T, std::initializer_list<U>>)
  void return_value(std::initializer_list<U> list) {
    result_ = T{list};
  }
  
  // Exception thrown in task body are stored in expected.
  void unhandled_exception() { 
    result_ = std::unexpected(std::current_exception());
  }

  ReturnValue&& value() && {
    return std::move(result_).value();  
  } 
  
  // Return a reference to stored value.
  // If the value is not set throwd std::bad_expected_acesss<>.
  ReturnValue& value() & {
    return result_.value();  
  } 

  std::exception_ptr& error() & noexcept {
    return result_.error();
  }

  ReturnValue& operator*() & noexcept {
    return *result_;
  }

  ReturnValue&& operator*() && noexcept {
    return *std::move(result_);
  }
  
  template<typename T>
  ReturnValue value_or(T&& default_value) & {
    return result_.value_or(std::forward<T>(default_value));
  }

  template<typename T>
  ReturnValue value_or(T&& default_value) && {
    return std::move(result_).value_or(std::forward<T>(default_value));
  }

  ResultType& expected() & noexcept {
    return result_;
  }

  ResultType&& expected() && noexcept {
    return  std::move(result_);
  }

  explicit operator bool() noexcept {
    return (bool)result_;
  }

   bool has_value() noexcept {
    return result_.has_value();
  }

  void set_continuation(std::coroutine_handle<> cont) noexcept { continuation_ = cont; }

 private:
  // When we co_await task, we need to store a coroutine handle of a coroutine we want to resume
  // when this tasks finishes. Allows for coroutine to coroutine transfer of control.
  // std::coroutine_handle<> continuation_ = nullptr;
  std::coroutine_handle<> continuation_ = std::noop_coroutine();
  // Stores either value or a exception that was thrown during coroutine's execution.
  std::expected<ReturnValue, std::exception_ptr> result_{std::unexpect_t{}};
};

template <>
class SimplePromise<void> {
 public:

  using ResultType = std::expected<void, std::exception_ptr>;

  Task<void> get_return_object() noexcept; 

  std::suspend_always initial_suspend() noexcept { return {}; }
  
  ResumeAwaiter final_suspend() noexcept {
    return ResumeAwaiter{continuation_};
  }
  
  // If the method returns void, and does not throw exception
  // we want the expected to has expected type void.
  void return_void() noexcept {
    result_.emplace();
  }

  void value() && {
    return;   
  } 
  
  // Return a reference to stored value.
  // If the value is not set throwd std::bad_expected_acesss<>.
  void value() const& {
    return;  
  } 

  void operator*() noexcept {
    return ;
  }
  
  // Exception thrown in task body are stored in expected.
  void unhandled_exception() { 
    result_ = std::unexpected(std::current_exception());
  }

  std::exception_ptr& error() & noexcept {
    return result_.error();
  }

  ResultType& expected() & noexcept {
    return result_;
  }

  ResultType&& expected() && noexcept {
    return  std::move(result_);
  }

  explicit operator bool() noexcept {
    return (bool)result_;
  }

  bool has_value() noexcept {
    return result_.has_value();
  }

  void set_continuation(std::coroutine_handle<> cont) noexcept { continuation_ = cont; }

 private:
  // When we co_await task, we need to store a coroutine handle of a coroutine we want to resume
  // when this tasks finishes. Allows for coroutine to coroutine transfer of control.
  // std::coroutine_handle<> continuation_ = nullptr;
  std::coroutine_handle<> continuation_ = std::noop_coroutine();
  // Stores either value or a exception that was thrown during coroutine's execution.
  std::expected<void, std::exception_ptr> result_{std::unexpect_t{}};
};

// This awaiter suspends current coroutine and sets contination to a coroutine, we passed 
// to the constructor. This way we can resume the task when it finishes. 
//
// task<T> foo() {
//   co_await bar();
//  }
//
// Suspends foo, sets foo as a contination to bar and resumes bar. 
template <typename PROMISE_TYPE>
class ContinuationSetAwaiter {
 public:

  constexpr bool await_ready() noexcept { return false; }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> currently_suspended) noexcept {
    to_resume_.promise().set_continuation(currently_suspended);
    return to_resume_;
  }

  // returns result of the whole co_await expresion
  void await_resume() noexcept {}

  std::coroutine_handle<PROMISE_TYPE> to_resume_;
};


// Needs to be defined when running tests. Allows for checking the number
// of task instances alive.
#ifdef COROS_TEST_
  #define INSTANCE_COUNTER_ : private coros::test::InstanceCounter<Task<ReturnValue>>
#else
  #define INSTANCE_COUNTER_ 
#endif


} // namespace detail

// Task is an object returned to the user when a coroutine is created. Allows users
// to get return value from the promise object.
template <TaskRetunType ReturnValue>
class Task INSTANCE_COUNTER_ {
 public:
  // Coroutines traits looks for promise_type.
  using promise_type = detail::SimplePromise<ReturnValue>;

  template<typename T = Task<ReturnValue>>
  static std::enable_if_t<std::is_base_of_v<coros::test::InstanceCounter<Task<ReturnValue>>, T>, std::size_t>
  instance_count() {
    return coros::test::InstanceCounter<Task<ReturnValue>>::instance_count();
  }

  // using InstanceCounter<Task<ReturnValue>>::instance_count;

  Task() : handle_(nullptr) {};

  Task(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle){ };

  Task(Task& other) = delete;

  Task(Task&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }

  Task& operator=(Task& other) = delete;

  Task& operator=(Task&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
    return *this; }

  ~Task() {
    if (handle_ != nullptr) {
      handle_.destroy();
    }
  }
  
  // When Task is co_awaited it is suspended and continuation to the calling
  // task is set, allowing for control transfer once the co_await-ed task is done.
  detail::ContinuationSetAwaiter<promise_type> operator co_await() noexcept {
    return detail::ContinuationSetAwaiter<promise_type>{handle_};
  }
  
  // Returns void.
  template<typename T = ReturnValue> 
  std::enable_if_t<std::is_void_v<T>>
  value() & { }
   
  // Return a value stored in expected. Can potentionally throw.
  template<typename T = ReturnValue> 
  std::enable_if_t<!std::is_void_v<T>, T>&
  value() & {
      return handle_.promise().value();
  }

  std::exception_ptr& error() & noexcept  {
    return handle_.promise().error();
  }

  template<typename T = ReturnValue> 
  std::enable_if_t<!std::is_void_v<T>, T>&
  operator*() & noexcept {
    return *handle_.promise();
  }

  template<typename T = ReturnValue> 
  std::enable_if_t<std::is_void_v<T>>
  operator*() & noexcept { }

  template<typename T = ReturnValue> 
  std::enable_if_t<!std::is_void_v<T>, T>&&
  operator*() && noexcept {
    return std::move(*handle_.promise());
  }

  template<typename T = ReturnValue> 
  std::enable_if_t<std::is_void_v<T>>
  operator*() && noexcept { }

  template<typename T>
  ReturnValue value_or(T&& default_value) & {
    return handle_.promise().value_or(std::forward<T>(default_value));
  }

  template<typename T>
  ReturnValue value_or(T&& default_value) && {
    return std::move(handle_.promise()).value_or(std::forward<T>(default_value));
  }

  std::expected<ReturnValue, std::exception_ptr>& expected() & noexcept {
    return handle_.promise().expected();
  }

  std::expected<ReturnValue, std::exception_ptr>&& expected() && noexcept {
    return std::move(handle_.promise()).expected();
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(handle_.promise().expected());
  }

  bool has_value() const noexcept {
    return handle_.promise().has_value();
  }

  std::coroutine_handle<promise_type> get_handle() const noexcept { return handle_; }

 private:
  std::coroutine_handle<promise_type> handle_ = nullptr;
};

// Placed here, because of forward declaration.
inline Task<void> detail::SimplePromise<void>::get_return_object() noexcept {
  return Task<void>{std::coroutine_handle<detail::SimplePromise<void>>::from_promise(*this)};
};

} // namespace coros

#endif  // COROS_INCLUDE_TASK_H_
