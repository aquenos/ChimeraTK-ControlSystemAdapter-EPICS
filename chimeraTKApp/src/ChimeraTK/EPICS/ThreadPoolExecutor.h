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

#ifndef CHIMERATK_EPICS_THREAD_POOL_EXECUTOR_H
#define CHIMERATK_EPICS_THREAD_POOL_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace ChimeraTK {
namespace EPICS {

/**
 * Fix-sized thread pool that can be used to execute arbitrary tasks.
 */
class ThreadPoolExecutor {

public:

  /**
   * Creates a thread pool of the specified size. If the size is less than one,
   * no tasks can be submitted to this thread pool.
   */
  explicit ThreadPoolExecutor(std::size_t numberOfPoolThreads);

  /**
   * Destroys this thread pool.
   *
   * The destructor internally calls shutdown(), so it may block until all
   * submitted tasks have been processed.
   */
  ~ThreadPoolExecutor();

  /**
   * Shuts down all threads in this thread pool. All submitted tasks are
   * processed and all threads are terminted before this method returns, so it
   * may block for a while.
   */
  void shutdown();

  /**
   * Submits a task for execution. The task is asynchronously executed by one of
   * the worker threads.
   *
   * Throws an exception if there are no worker threads or if this thread pool
   * is being shut down.
   */
  template<typename Function, typename... Args>
  std::future<typename std::result_of<typename std::decay<Function>::type(typename std::decay<Args>::type...)>::type> submitTask(
      Function &&f, Args &&...args);

private:

  /**
   * Mutex protecting access to the tasks queue and the shutdownRequested flag.
   */
  std::mutex mutex;

  /**
   * Flag indicating whether shutdown() has been called.
   */
  bool shutdownRequested;

  /**
   * Tasks that have been submitted, but are not running yet.
   */
  std::queue<std::packaged_task<void()>> tasks;

  /**
   * Condition variable that is used to notify the pool threads. The pool
   * threads are notified when a new task is submitted or when this pool is
   * destroyed.
   */
  std::condition_variable tasksCv;

  /**
   * Vector of threads that can run tasks.
   */
  std::vector<std::thread> threads;

  // Delete copy constructors and assignment operators.
  ThreadPoolExecutor(ThreadPoolExecutor const&) = delete;
  ThreadPoolExecutor(ThreadPoolExecutor &&) = delete;
  ThreadPoolExecutor &operator=(ThreadPoolExecutor const&) = delete;
  ThreadPoolExecutor &operator=(ThreadPoolExecutor &&) = delete;

  /**
   * Processes tasks. This method is called by each of the threads created by
   * the constructor.
   */
  void runThread();

};

template<typename Function, typename... Args>
std::future<typename std::result_of<typename std::decay<Function>::type(typename std::decay<Args>::type...)>::type> ThreadPoolExecutor::submitTask(
    Function &&f, Args &&...args) {
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
  {
    // We do these checks that late because we need to acquire the mutex for
    // them and we want to hold the lock as shortly as possible.
    std::lock_guard<std::mutex> lock(this->mutex);
    if (this->shutdownRequested) {
      throw std::runtime_error(
        "Tasks cannot be submitted to a thread pool that has been or is being shut down.");
    }
    if (this->threads.size() == 0) {
      throw std::runtime_error(
        "Tasks cannot be submitted to a thread pool that does not have any threads.");
    }
    this->tasks.push(std::move(voidTask));
  }
  this->tasksCv.notify_one();
  return future;
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_THREAD_POOL_EXECUTOR_H
