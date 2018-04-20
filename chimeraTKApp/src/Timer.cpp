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

#include "ChimeraTK/EPICS/Timer.h"

namespace ChimeraTK {
namespace EPICS {

Timer::Timer() : impl(std::make_shared<Impl>()) {
}

void Timer::Impl::runThread() {
  for (;;) {
    std::packaged_task<void()> nextTask;
    {
      std::unique_lock<std::mutex> lock(this->mutex);
      if (this->tasks.empty()) {
        this->threadRunning = false;
        break;
      }
      while (Clock::now() < this->tasks.top().time) {
        this->tasksCv.wait_until(lock, this->tasks.top().time);
      }
      // The const_cast is a bit ugly, but there is no other way to move an
      // element from a priority queue (pop() returns void). As we only
      // modify a field not used in comparison, the priority_queue should not
      // care about us modifying the stored object.
      nextTask = std::move(this->tasks.top().task);
      this->tasks.pop();
    }
    nextTask();
  }
}

void Timer::Impl::submitTask(Task &&task) {
  {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->tasks.push(std::move(task));
    // If no thread is running, we have to start one.
    if (!this->threadRunning) {
      // We use a shared pointer to this instead of this so that this object
      // cannot be destructed while the thread is running.
      auto impl = this->shared_from_this();
      std::thread t([impl]{ impl->runThread(); });
      this->threadRunning = true;
      t.detach();
    }
  }
  this->tasksCv.notify_one();
}

// Initializer for the static storage member.
Timer Timer::sharedInstance;

} // namespace EPICS
} // namespace ChimeraTK
