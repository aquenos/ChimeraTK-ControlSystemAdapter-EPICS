/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2018 aquenos GmbH
 *
 * The ChimeraTK Control System Adapter for EPICS is free software: you can
 * redistribute it and/or modify it under the terms of the GNU Lesser General
 * Public License version 3 as published by the Free Software Foundation.
 *
 * The ChimeraTK Control System Adapter for EPICS is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the ChimeraTK Control System Adapter for EPICS. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CHIMERATK_EPICS_TIMER_H
#define CHIMERATK_EPICS_TIMER_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace ChimeraTK {
namespace EPICS {

/**
 * Timer that allows for delayed execution of tasks. The timer internally
 * creates a thread that executes tasks. This thread is only created when tasks
 * are actually submitted to it and terminates when there are no more tasks.
 *
 * A shared instance of this class is returned by its static shared() method.
 */
class Timer {

public:

  /**
   * Returns a reference to a shared timer instance. This timer is suitable for
   * use with short-running tasks where congestion of the timer thread is not an
   * issue.
   */
  inline static Timer &shared() {
    return sharedInstance;
  }

  /**
   * Creates a timer.
   */
  Timer();

  /**
   * Destroys this timer.
   *
   * If there is an active timer thread, it is only detached, not terminated. It
   * will terminate when the last queued task has been processed.
   */
  ~Timer() = default;

  /**
   * Submits a task for execution. The task is asynchronously executed, but only
   * after at least the specified amount of time has passed. If there are other
   * tasks that are blocking the execution thread, the execution might be
   * delayed significantly longer than specified.
   */
  template<typename Rep, typename Period, typename Function, typename... Args>
  std::future<typename std::result_of<typename std::decay<Function>::type(typename std::decay<Args>::type...)>::type> submitDelayedTask(
      std::chrono::duration<Rep, Period> const &delay,
      Function &&f,
      Args &&...args);

private:

  /**
   * Type of the clock that we use. We prefer a steady clock because we deal
   * with delays.
   */
  using Clock = std::chrono::steady_clock;

  /**
   * Type of the time points that we use.
   */
  using TimePoint = std::chrono::time_point<Clock>;

  /**
   * Task for execution. This basically is a wrapper around the function that
   * shall be executed, complimented by a time at or after which it shall be
   * executed.
   *
   * This class implements the less-than operator so that the task with the
   * earliest time is the greatest task.
   */
  struct Task {

    /**
     * Task that shall be executed. The field is marked as mutable so that we
     * can move from it, even though the priority_queue's top() method only
     * returns a const reference.
     */
    mutable std::packaged_task<void()> task;

    /**
     * Time at or after which this task shall be executed.
     */
    TimePoint time;

    /**
     * Compares two tasks according to their times.
     */
    inline bool operator<(Task const &other) const {
      // We intentionally use the greater than operator here. The task with the
      // least priority is the one with the greatest time.
      return this->time > other.time;
    }

  };

  /**
   * Actual implementation class. This implementation is separated from the
   * visible interface so that it can survive past the destruction of the
   * visible instance.
   */
  struct Impl : std::enable_shared_from_this<Impl> {

    /**
     * Mutex protecting access to the tasks queue.
     */
    std::mutex mutex;

    /**
     * Tasks that have been submitted, but are not running yet.
     */
    std::priority_queue<Task> tasks;

    /**
     * Condition variable that is used to notify the pool threads. The pool
     * threads are notified when a new task is submitted or when this pool is
     * destroyed.
     */
    std::condition_variable tasksCv;

    /**
     * Flag indicating whether there currently is a running thread.
     */
    bool threadRunning = false;

    /**
     * Processes tasks. This method is called by each of the threads created by
     * the constructor.
     */
    void runThread();

    /**
     * Add a task to the queue and start a thread if necessary.
     */
    void submitTask(Task &&task);

  };

  static Timer sharedInstance;

  /**
   * We keep a shared pointer to the implementation. This ensures that the
   * implementation survives when this object is destroyed and is only destroyed
   * after all tasks have been processed.
   */
  std::shared_ptr<Impl> impl;

  // Delete copy constructors and assignment operators.
  Timer(Timer const &) = delete;
  Timer(Timer &&) = delete;
  Timer &operator=(Timer const &) = delete;
  Timer &operator=(Timer &&) = delete;

};

template<typename Rep, typename Period, typename Function, typename... Args>
std::future<typename std::result_of<typename std::decay<Function>::type(typename std::decay<Args>::type...)>::type> Timer::submitDelayedTask(
    std::chrono::duration<Rep, Period> const &delay,
    Function &&f,
    Args &&...args) {
  using T = typename std::result_of<typename std::decay<Function>::type(typename std::decay<Args>::type...)>::type;
  auto task = std::packaged_task<T()>(
    std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
  auto future = task.get_future();
  // We have to package the packaged_task in another packaged_task so that
  // all tasks on the queue have the same return type.
  auto voidTask = std::packaged_task<void()>(
    [task = std::move(task)]() mutable {
      task();
    });
  this->impl->submitTask(Task{std::move(voidTask), Clock::now() + delay});
  return future;
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_TIMER_H
