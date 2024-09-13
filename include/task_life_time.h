#ifndef COROS_INCLUDE_TASK_LIFE_TIME_H_
#define COROS_INCLUDE_TASK_LIFE_TIME_H_

namespace coros {
namespace detail {
  
// Thread pool managed tasks are destroyed when the thread pool is destroyed.
enum class TaskLifeTime {
  SCOPE_MANAGED,
  THREAD_POOL_MANAGED,
  NOOP, /*Used with noop coroutines*/
};
  
} // namespace detail
} // namespace coros

#endif  // COROS_INCLUDE_TASK_LIFE_TIME_H_
