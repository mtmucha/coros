#ifndef COROS_INCLUDE_CHAIN_TASKS_H_
#define COROS_INCLUDE_CHAIN_TASKS_H_

#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "task.h"

namespace coros {
namespace detail {

template <typename T>
struct extract_return_type;

template <typename R, typename U>
struct extract_return_type<coros::Task<R>(*)(U)> {
    using type = R;
};

template <typename T>
using extract_return_type_t = typename extract_return_type<T>::type;

template <typename T>
concept FunctionNoParams = requires {
    requires std::is_function_v<T>;
    requires std::is_same_v<T, void()> || std::is_same_v<T, void(*)()>;
};

template<typename T>
struct is_Task : std::false_type{};

template<typename T>
struct is_Task<coros::Task<T>> : std::true_type{};

template<typename T>
concept IsTask = is_Task<T>::value;

template<typename T>
concept IsNotTask = !is_Task<T>::value;

//
// Forward declarations.
//

template<typename T, typename U, typename F, typename... Funcs>
requires (!FunctionNoParams<F> && IsNotTask<U>)
coros::Task<T> process_tasks(U val, F f, Funcs... functions);

template<typename T, IsNotTask U, typename F>
requires (!FunctionNoParams<F> && IsNotTask<U>)
coros::Task<T> process_tasks(U val, F f);

template<typename... Funcs>
coros::Task<void> process_tasks(coros::Task<void>(*f)(), Funcs... functions);

inline coros::Task<void> process_tasks(coros::Task<void>(*f)());

template<typename... Funcs>
coros::Task<void> process_tasks(coros::Task<void>(*f)());

template<typename T, typename U, typename... Funcs>
coros::Task<T> process_tasks(coros::Task<U>& starting_task, Funcs... functions);

template<typename T, typename U>
coros::Task<T> process_tasks(coros::Task<U>& starting_task);

//
// Function definitions.
//

template<typename T, typename U, typename F, typename... Funcs>
requires (!FunctionNoParams<F> && IsNotTask<U>)
coros::Task<T> process_tasks(U val, F f, Funcs... functions) {

  // Extract the return type from the pointer
  using return_type = extract_return_type_t<F>;
  
  // Construct the promise object and task object
  coros::Task<return_type> t = f(std::move(val));
  
  // Execute the function
  co_await t;
  
  // An error occured. Should return unexpected
  // Rethrown exception will be caught, propagation
  // of the exception.
  if (!t.has_value()) {
    std::rethrow_exception(t.error());
  }   
  
  // TODO : Do not like this, should be more readable.
  if constexpr (std::is_void_v<return_type>) {
    auto next_task = coros::detail::process_tasks(functions...);
    co_await next_task;
    
    if (!next_task.has_value()) {
      std::rethrow_exception(next_task.error());
    }

    co_return;
  } else {
    auto next_task = coros::detail::process_tasks<T>(*std::move(t), functions...);
    co_await next_task;

    if (!next_task.has_value()) {
      std::rethrow_exception(next_task.error());
    }

    if constexpr (std::is_void_v<T>) {
      co_return;
    } else{
      co_return *std::move(next_task);
    }
  }
}

// Base case
template<typename T, IsNotTask U, typename F>
requires (!FunctionNoParams<F> && IsNotTask<U>)
coros::Task<T> process_tasks(U val, F f) {
  // Extract the return type from the pointer
  using return_type = extract_return_type_t<F>;
  
  // Construct the promise object and task object
  coros::Task<return_type> t = f(std::move(val));
  
  // Execute the function
  co_await t;

  // An error occurred. Should return unexpected
  // Rethrown exception will be caught, propagation
  // of the exception.
  if (!t.has_value()) {
    std::rethrow_exception(t.error());
  }   

  if constexpr (std::is_void_v<T>) {
    co_return;
  } else{
    co_return *std::move(t);
  }
}

// TODO : Maybe fix the concept.
template<typename... Funcs>
coros::Task<void> process_tasks(coros::Task<void>(*f)(), Funcs... functions) {
  coros::Task<void> t = f();
  co_await t;
  
  if (!t.has_value()) {
    std::rethrow_exception(t.error());
  }   

  auto next_task = coros::detail::process_tasks(functions...);
  co_await next_task;

  if (!next_task.has_value()) {
    std::rethrow_exception(next_task.error());
  }

  co_return;
}

inline coros::Task<void> process_tasks(coros::Task<void>(*f)()) {
  coros::Task<void> t = f();
  co_await t;


  if (!t.has_value()) {
    std::rethrow_exception(t.error());
  }   

  co_return;
}

template<typename T, typename U, typename... Funcs>
coros::Task<T> process_tasks(coros::Task<U>& starting_task, Funcs... functions) {
  // Execute the first task.
  co_await starting_task;

  if (!starting_task.has_value()) {
    std::rethrow_exception(starting_task.error());
  }   
  
  if constexpr (std::is_void_v<U>) {
    auto next_task = coros::detail::process_tasks(functions...);
    co_await next_task;
    
    if (!next_task.has_value()) {
      std::rethrow_exception(next_task.error());
    }

    co_return;
  } else {
    auto next_task = coros::detail::process_tasks<T>(*std::move(starting_task), functions...);
    co_await next_task;

    if (!next_task.has_value()) {
      std::rethrow_exception(next_task.error());
    }

    if constexpr (std::is_void_v<T>) {
      co_return;
    } else{
      co_return *next_task;
    }
  }
}

// Base case
template<typename T, typename U>
coros::Task<T> process_tasks(coros::Task<U>& starting_task) {
  
  // Execute the function
  co_await starting_task;

  // An error occurred. Should return unexpected
  // Rethrown exception will be caught, propagation
  // of the exception.
  if (!starting_task.has_value()) {
    std::rethrow_exception(starting_task.error());
  }   

  if constexpr (std::is_void_v<T>) {
    co_return;
  } else{
    co_return *starting_task;
  }
}

} // namespace detail


// Template arguments:
//   
//   StartingType - Type that is passed to the constructor of the awaitable.
//   EndingType - Type that is returned to the user, final expected returned to 
//                the user is std::expected<EndingType, std::exception_ptr>.
//   TaskStart - Set to true if the awaitable is passed a task to execute
//               rather than a value.
//   Funcs - Individual accumulated functions that should be executed on the value
//           when the awaitable is co_await-ed.
template<
  typename StartingType, 
  typename EndingType = StartingType,
  bool TaskStart = false,
  typename... Funcs>
class ChainAwaitable {
 public:

  ChainAwaitable(std::expected<StartingType, std::exception_ptr>&& expected) 
      : expected_(std::move(expected)) {}

  ChainAwaitable(std::expected<StartingType, std::exception_ptr>& expected) 
      : expected_(expected) {}

  ChainAwaitable(coros::Task<StartingType>&& starting_task) 
      : starting_task_(std::move(starting_task)) {}
  
  template<typename U>
  ChainAwaitable(U&& val) 
      : expected_({std::forward<U>(val)}) {}


  explicit ChainAwaitable(
    std::expected<StartingType, std::exception_ptr>&& ex, 
    std::tuple<Funcs...>&& tuple) 
      : expected_(std::move(ex)),
        functions_(std::move(tuple)) {}

  explicit ChainAwaitable(
    std::expected<StartingType, std::exception_ptr>&& ex, 
    std::tuple<Funcs...>&& tuple,
    coros::Task<StartingType>&& starting_task)
      : expected_(std::move(ex)),
        functions_(std::move(tuple)),
        starting_task_(std::move(starting_task)) {}

  // TODO : implement later
  // HACK : Getting some weird behavior with parsing the template,
  // therefor I use AwaiterVoid.
  auto operator co_await() {
    if constexpr (std::is_void_v<EndingType>) {
      return AwaiterVoid{this};
    } else {
      return Awaiter<EndingType>{this};
    }
  }


  template<typename FinalResultType>
  struct Awaiter {
    
    constexpr bool await_ready() noexcept {return false;}
    
    // Here I should suspend, and run a task that runs individaul functions.
    std::coroutine_handle<> 
    await_suspend(std::coroutine_handle<> currently_suspended) noexcept {
      loop_task_.get_handle().promise().set_continuation(currently_suspended);
      return loop_task_.get_handle();
    }
    

    [[nodiscard]] std::expected<FinalResultType, std::exception_ptr>&& await_resume() noexcept {
      return std::move(loop_task_).expected();
    }

    ChainAwaitable* awaitable_;
    // Needed to use lambda to make it work. Not sure why.
    coros::Task<FinalResultType> loop_task_ =
       std::apply(
          [](auto&&... args) {
              return coros::detail::process_tasks<FinalResultType>(std::forward<decltype(args)>(args)...);
          },
          [this]() -> auto {
            if constexpr (!TaskStart) {
              return std::tuple_cat(
                       std::make_tuple(std::move(awaitable_->expected_).value()), 
                       awaitable_->functions_);
            } else {
              return std::tuple_cat(
                       std::tie(awaitable_->starting_task_), 
                       awaitable_->functions_);
            }
          }()
      );
  };

  struct AwaiterVoid {
    
    constexpr bool await_ready() noexcept {return false;}
    
    // Here I should suspend, and run a task that runs individaul functions.
    std::coroutine_handle<> 
    await_suspend(std::coroutine_handle<> currently_suspended) noexcept {
      loop_task_.get_handle().promise().set_continuation(currently_suspended);
      return loop_task_.get_handle();
    }
    
    [[nodiscard]] std::expected<void, std::exception_ptr>&& await_resume() noexcept {
      return std::move(loop_task_).expected();
    }

    ChainAwaitable* awaitable_;
    // Needed to use lambda to make it work. Not sure why.
    coros::Task<void> loop_task_ =
       std::apply(
          [](auto&&... args) {
              return coros::detail::process_tasks<void>(std::forward<decltype(args)>(args)...);
          },
          [this]() -> auto {
            if constexpr (!TaskStart) {
              return std::tuple_cat(
                       std::make_tuple(std::move(awaitable_->expected_).value()), 
                       awaitable_->functions_);
            } else {
              return std::tuple_cat(
                       std::tie(awaitable_->starting_task_), 
                       awaitable_->functions_);
            }
          }()
      );
  };
  
  // Should create a new ChainAwaitable and add the chaining function into the list.
  // We do not stop here but in chaining function. This function checks whether the
  // types are valid. Meaning, the function takes the input type currently stored in ChainAwaitable.
  // Could this be done in constant time. I think so !
  template <typename R, typename U>
  constexpr auto and_then(coros::Task<R>(*fun)(U)) {
    // TODO: Should remove the std::function ?
    static_assert(
      std::is_invocable_v<std::function<coros::Task<R>(U)>, EndingType>, 
      "Task in and_then() needs to accept corret type");

    // Add and_then into a tuple
    // functions.tuple_cat(std::make_tuple(fun));
    auto new_tuple = std::tuple_cat(functions_, std::make_tuple(fun));
    
    // Need to create a new expected, with a new type.
    if constexpr (TaskStart) {
      return ChainAwaitable<StartingType, R, TaskStart, Funcs..., coros::Task<R>(*)(U)>{
        std::move(expected_),
        std::move(new_tuple),
        std::move(starting_task_)};
    } else {
      return ChainAwaitable<StartingType, R, TaskStart, Funcs..., coros::Task<R>(*)(U)>{
        std::move(expected_),
        std::move(new_tuple)};
    }
  }

  constexpr auto and_then(coros::Task<void>(*fun)()) {
    // Add and_then into a tuple
    // functions.tuple_cat(std::make_tuple(fun));
    auto new_tuple = std::tuple_cat(functions_, std::make_tuple(fun));
    
    // Need to create a new expected, with a new type.
    if constexpr (TaskStart) {
      return ChainAwaitable<StartingType, void, TaskStart, Funcs..., coros::Task<void>(*)()>{
        std::move(expected_),
        std::move(new_tuple),
        std::move(starting_task_)};
    } else {
      return ChainAwaitable<StartingType, void, TaskStart, Funcs..., coros::Task<void>(*)()>{
        std::move(expected_),
        std::move(new_tuple)};
    }
  }
   
 private:
  std::expected<StartingType, std::exception_ptr> expected_;
  std::tuple<Funcs...> functions_;
  coros::Task<StartingType> starting_task_;
};


template<typename T>
ChainAwaitable<T, T, false> chain_tasks(std::expected<T, std::exception_ptr>& ex) { 
  return ChainAwaitable<T, T, false>{ex};
}

template<typename T>
ChainAwaitable<T, T, false> chain_tasks(std::expected<T, std::exception_ptr>&& ex) { 
  return ChainAwaitable<T, T, false>{std::move(ex)};
}

template<typename T>
ChainAwaitable<T, T, true> chain_tasks(coros::Task<T>&& task) { 
  return ChainAwaitable<T, T, true>{std::move(task)};
}

template<typename T>
requires(
  !std::is_same_v<std::expected<std::remove_reference_t<T>, std::exception_ptr>, 
                  std::remove_reference_t<T>>
  &&std::is_constructible_v<std::expected<std::remove_reference_t<T>, 
                            std::exception_ptr>, std::remove_reference_t<T>>) 
ChainAwaitable<std::remove_reference_t<T>, std::remove_reference_t<T>, false> chain_tasks(T&& val) {
  return ChainAwaitable<std::remove_reference_t<T>, 
                        std::remove_reference_t<T>, 
                        false>
                        {std::forward<T&>(val)};
}

} // namespace coros

#endif  // COROS_INCLUDE_CHAIN_TASKS_H_
