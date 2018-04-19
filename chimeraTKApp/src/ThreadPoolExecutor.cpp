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

#include "ChimeraTK/EPICS/ThreadPoolExecutor.h"

namespace ChimeraTK {
namespace EPICS {

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t numberOfPoolThreads)
    : shutdownRequested(false) {
  for (std::size_t i = 0; i < numberOfPoolThreads; ++i) {
    this->threads.push_back(std::thread([this](){this->runThread();}));
  }
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
  shutdown();
}

void ThreadPoolExecutor::runThread() {
  for (;;) {
    std::function<void ()> nextTask;
    {
      std::unique_lock<std::mutex> lock(this->mutex);
      if (this->tasks.empty()) {
        if (this->shutdownRequested) {
          break;
        } else {
          this->tasksCv.wait(lock);
          continue;
        }
      }
      nextTask = std::move(this->tasks.front());
      this->tasks.pop();
    }
    nextTask();
  }
}

void ThreadPoolExecutor::shutdown() {
  {
    std::unique_lock<std::mutex> lock(this->mutex);
    // If another thread already called shutdown, we have to wait until the
    // shutdown has finished. We abuse the tasksCv for this purpose.
    while (this->threads.size() != 0 && this->shutdownRequested) {
      this->tasksCv.wait(lock);
    }
    if (this->threads.size() == 0) {
      // We always set shutdownRequested here. If it has already been requested
      // earlier, this has no effects. If it has not because this thread pool
      // was created with no threads, it makes sure that this thread pool is
      // marked as shut down.
      this->shutdownRequested = true;
      return;
    }
    // If the number of threads is not zero, this is the first thread in which
    // shutdown has been called.
    this->shutdownRequested = true;
  }
  // We notify all threads in case some of them are sleeping.
  this->tasksCv.notify_all();
  // We help these threads in processing any remaining tasks.
  for (;;) {
    std::function<void ()> nextTask;
    {
      std::lock_guard<std::mutex> lock(this->mutex);
      if (this->tasks.empty()) {
        break;
      }
      nextTask = std::move(this->tasks.front());
      this->tasks.pop();
    }
    nextTask();
  }
  // If we are here, the queue is empty, but there might still be some
  // running tasks. We wait until these tasks have finished as well.
  for (auto &thread : this->threads) {
    thread.join();
  }
  // Now all threads have terminated, so we can remove them from the vector.
  {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->threads.clear();
  }
  // Finally, we notify any threads which might also have called shutdown().
  this->tasksCv.notify_all();
}

} // namespace EPICS
} // namespace ChimeraTK
