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

#include <cstdint>
#include <stdexcept>

#include <ChimeraTK/RegisterPath.h>

#include "ChimeraTK/EPICS/ControlSystemAdapterSharedPVSupportImpl.h"

#include "ChimeraTK/EPICS/ControlSystemAdapterPVProvider.h"

namespace ChimeraTK {
namespace EPICS {

ControlSystemAdapterPVProvider::ControlSystemAdapterPVProvider(
    ControlSystemPVManager::SharedPtr const & pvManager,
    std::chrono::microseconds pollingInterval)
    : pollingInterval(pollingInterval), pollingThreadShutdownRequested(false),
      pvManager(pvManager) {
  this->insertCreatePVSupportFunc<std::int8_t>();
  this->insertCreatePVSupportFunc<std::uint8_t>();
  this->insertCreatePVSupportFunc<std::int16_t>();
  this->insertCreatePVSupportFunc<std::uint16_t>();
  this->insertCreatePVSupportFunc<std::int32_t>();
  this->insertCreatePVSupportFunc<std::uint32_t>();
  this->insertCreatePVSupportFunc<float>();
  this->insertCreatePVSupportFunc<double>();
  this->pollingThread = std::thread([this]{this->runPollingThread();});
}

std::type_info const &ControlSystemAdapterPVProvider::getDefaultType(
    std::string const &processVariableName) {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  return this->pvManager->getProcessVariable(processVariableName)
      ->getValueType();
}

ControlSystemAdapterPVProvider::~ControlSystemAdapterPVProvider() {
  {
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    this->pollingThreadShutdownRequested = true;
  }
  this->pollingThreadCv.notify_all();
  this->pollingThread.join();
}

PVSupportBase::SharedPtr ControlSystemAdapterPVProvider::createPVSupport(
    std::string const &processVariableName,
    std::type_info const& elementType) {
  try {
    auto createFunc = this->createPVSupportFuncs.at(
        std::type_index(elementType));
    return (this->*createFunc)(processVariableName);
  } catch (std::out_of_range &e) {
    throw std::runtime_error(
        std::string("The element type '") + elementType.name()
            + "' is not supported.");
  }
}

template<typename T>
PVSupportBase::SharedPtr ControlSystemAdapterPVProvider::createPVSupportInternal(
    std::string const &processVariableName) {
  // We normalize the PV name so that names that look different but actually
  // represent the same PV get resolved to the same shared PV support instance.
  std::string name = RegisterPath(processVariableName);
  std::shared_ptr<ControlSystemAdapterSharedPVSupportBase> shared;
  // We try to find an existing shared instance. If we cannot find an entry for
  // the process variable name or the weak pointer of the entry has expired, we
  // create a new instance.
  auto sharedIter = this->sharedPVSupports.find(name);
  if (sharedIter != this->sharedPVSupports.end()) {
    shared = sharedIter->second.lock();
  }
  if (!shared) {
    // If the weak pointer has expired, we have to remove the old entry from the
    // map so that a new entry can be inserted.
    if (sharedIter != this->sharedPVSupports.end()) {
      this->sharedPVSupports.erase(sharedIter);
    }
    // If the desired type is not supported for the specified process variable,
    // the constructor called by make_shared will thrown an exception. This is
    // exactly what we want, but we prefer an std::invalid_argument exception.
    try {
      shared = std::make_shared<ControlSystemAdapterSharedPVSupport<T>>(
          this->shared_from_this(), name);
    } catch (std::bad_cast &e) {
      throw std::invalid_argument(
          std::string("The type '") + typeid(T).name()
              + "' is not supported for the process variable '"
              + name + "'.");
    }
    this->sharedPVSupports.insert(std::make_pair(name, shared));
  }
  auto typedShared =
      std::dynamic_pointer_cast<ControlSystemAdapterSharedPVSupport<T>>(shared);
  // If the shared instance is of the wrong type, the requested type is
  // incorrect because the control system adapter supports exactly one type for
  // each process variable. We throw a bad_cast exception because this is the
  // same exception that is also thrown when trying to create a shared instance
  // using the wrong type for the first time.
  if (!typedShared) {
    throw std::invalid_argument(
        std::string("The type '") + typeid(T).name()
            + "' is not supported for the process variable '"
            + name + "'.");
  }
  return typedShared->createPVSupport();
}

template<typename T>
void ControlSystemAdapterPVProvider::insertCreatePVSupportFunc() {
  this->createPVSupportFuncs.insert(
      std::make_pair(
          std::type_index(typeid(T)),
          &ControlSystemAdapterPVProvider::createPVSupportInternal<T>));
}

void ControlSystemAdapterPVProvider::runPollingThread() {
  // We cannot check the abort condition here because we have to hold a lock on
  // the mutex while checking the condition.
  bool waitInNextIteration = false;
  while (true) {
    std::forward_list<std::function<void()>> notifyFunctions;
    // We limit the code where we hold the mutex to the part where it is really
    // needed. In particular, we do not want to hold the lock when calling
    // notification callbacks as this could result in a deadlock in the worst
    // case.
    {
      std::unique_lock<std::recursive_mutex> lock(this->mutex);
      // If a shutdown has been requested, we quit immediately.
      if (this->pollingThreadShutdownRequested) {
        return;
      }
      // We do the waiting here instead of at the end of the loop because we
      // need to hold a lock on the mutex in order to do the waiting and we do
      // not want to acquire the lock twice.
      if (waitInNextIteration) {
        this->pollingThreadCv.wait_for(lock, this->pollingInterval);
        // The shutdown flag might have been changed while we were sleeping, so
        // we check it again.
        if (this->pollingThreadShutdownRequested) {
          return;
        }
      }
      // We set the waitInNextIteration flag here and reset it if we do anything
      // meaningful.
      waitInNextIteration = true;
      while (!this->pendingCallNotify.empty()) {
        waitInNextIteration = false;
        std::shared_ptr<ControlSystemAdapterSharedPVSupportBase> sharedPVSupport(
            std::move(this->pendingCallNotify.front()));
        this->pendingCallNotify.pop_front();
        if (sharedPVSupport->readyForNextNotification()) {
          auto notifyFunction = sharedPVSupport->doNotify();
          if (notifyFunction) {
            notifyFunctions.push_front(std::move(notifyFunction));
          }
        }
      }
      // We limit the number of events that we process before actually calling
      // the callbacks. In theory, it is possible that events arrive faster than
      // we can process them. If we did not have a limit, the list would grow
      // and grow until we run out of memory.
      int numberOfPVsWithEvent = 0;
      ProcessVariable::SharedPtr pvWithEvent;
      while (numberOfPVsWithEvent < 1000
          && (pvWithEvent = this->pvManager->nextNotification())) {
        ++numberOfPVsWithEvent;
        waitInNextIteration = false;
        auto &name = pvWithEvent->getName();
        try {
          auto sharedPVSupport = this->sharedPVSupports.at(name).lock();
          if (sharedPVSupport && sharedPVSupport->readyForNextNotification()) {
            auto notifyFunction = sharedPVSupport->doNotify();
            if (notifyFunction) {
              notifyFunctions.push_front(std::move(notifyFunction));
            }
          }
        } catch (std::out_of_range &e) {
          // If the notification is for a PV support that does not exist any
          // longer, we simply ignore it.
        }
      }
    }
    // After releasing the lock, we call the notify functions. It is important
    // that we do not do this while holding the lock because we would risk a
    // deadlock.
    for (auto &notifyFunction : notifyFunctions) {
      notifyFunction();
    }
  }

}

void ControlSystemAdapterPVProvider::scheduleCallNotify(
    std::shared_ptr<ControlSystemAdapterSharedPVSupportBase> pvSupport) {
  // This method is only called while already holding a lock on the mutex.
  this->pendingCallNotify.push_front(std::move(pvSupport));
  this->pollingThreadCv.notify_all();
}

} // namespace EPICS
} // namespace ChimeraTK
