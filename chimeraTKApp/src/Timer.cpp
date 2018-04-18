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
    std::function<void ()> nextTask;
    {
      std::unique_lock<std::mutex> lock(this->mutex);
      if (this->tasks.empty()) {
        this->threadRunning = false;
        break;
      }
      while (Clock::now() < this->tasks.top().time) {
        this->tasksCv.wait_until(lock, this->tasks.top().time);
      }
      nextTask = this->tasks.top().func;
      this->tasks.pop();
    }
    nextTask();
  }
}

void Timer::Impl::submitTask(Task const &task) {
  {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->tasks.push(task);
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
