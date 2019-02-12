/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2018-2019 aquenos GmbH
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
#include <string>

#include <ChimeraTK/RegisterPath.h>

#include "ChimeraTK/EPICS/ControlSystemAdapterSharedPVSupportImpl.h"

#include "ChimeraTK/EPICS/ControlSystemAdapterPVProvider.h"

namespace ChimeraTK {
namespace EPICS {

namespace {
  using cppext::future_queue;
} // anonymous namespace

ControlSystemAdapterPVProvider::ControlSystemAdapterPVProvider(
    ControlSystemPVManager::SharedPtr const & pvManager)
    : notificationThreadShutdownRequested(false),
      pvManager(pvManager), wakeUpQueue(1) {
  this->insertCreatePVSupportFunc<std::int8_t>();
  this->insertCreatePVSupportFunc<std::uint8_t>();
  this->insertCreatePVSupportFunc<std::int16_t>();
  this->insertCreatePVSupportFunc<std::uint16_t>();
  this->insertCreatePVSupportFunc<std::int32_t>();
  this->insertCreatePVSupportFunc<std::uint32_t>();
  this->insertCreatePVSupportFunc<float>();
  this->insertCreatePVSupportFunc<double>();
  this->insertCreatePVSupportFunc<std::string>();
  // We have to create a vector of all PVs that support notifications. We have
  // to create this vector here because it is needed in the notification thread
  // and when creating PV supports, so we would need extra synchronization if we
  // created it in the notification thread.
  for (auto &pv : this->pvManager->getAllProcessVariables()) {
    // We can only support notifications if the PV supports the
    // wait_for_new_data access mode. For other PVs, the I/O Intr mode is
    // simply not supported and one can only read the latest value by polling.
    if (pv->isReadable()
        && pv->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
      this->pvsForNotification.push_back(pv);
    }
  }
  this->sharedPVSupportsByIndex.resize(this->pvsForNotification.size());
  this->notificationThread =
      std::thread([this]{this->runNotificationThread();});
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
    this->notificationThreadShutdownRequested = true;
    this->wakeUpNotificationThread();
  }
  this->notificationThread.join();
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
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
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
    // We have to determine the index of the PV. Iterating over the list of all
    // PVs does not look terribly efficient, but as creating a new shared PV
    // support is a rare occassion, maintaining a hash map that maps PV names
    // to indices does not seem worth the effort.
    std::size_t index;
    for (index = 0; index < pvsForNotification.size(); ++index) {
      if (name == pvsForNotification[index]->getName()) {
        break;
      }
    }
    // If the desired type is not supported for the specified process variable,
    // the constructor called by make_shared will thrown an exception. This is
    // exactly what we want, but we prefer an std::invalid_argument exception.
    try {
      shared = std::make_shared<ControlSystemAdapterSharedPVSupport<T>>(
          this->shared_from_this(), name, index);
    } catch (std::bad_cast &e) {
      throw std::invalid_argument(
          std::string("The type '") + typeid(T).name()
              + "' is not supported for the process variable '"
              + name + "'.");
    }
    this->sharedPVSupports.insert(std::make_pair(name, shared));
    // Only PV supports for PVs that support notifications have a proper index,
    // so we have to check that the index is valid before inserting the PV
    // support into the vector.
    if (index < this->sharedPVSupportsByIndex.size()) {
      this->sharedPVSupportsByIndex[index] = shared;
    }
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

void ControlSystemAdapterPVProvider::runNotificationThread() {
  cppext::future_queue<std::size_t> notificationQueue;
  std::size_t wakeUpQueueIndex;
  // We have to create a vector of future queues for all process variables that
  // support notifications. We will use these queues in the notification thread.
  // We create that vector inside a block because we do not need it any longer
  // after creating the result future queue.
  {
    std::vector<cppext::future_queue<void>> pvNotificationQueues;
    for (auto &pv : this->pvsForNotification) {
      auto &transferFuture = pv->readAsync();
      pvNotificationQueues.push_back(
          ChimeraTK::detail::getFutureQueueFromTransferFuture(transferFuture));
    }
    // As the last element, we append a special future queue that we can use to
    // wake up the notification thread when we have to.
    wakeUpQueueIndex = pvNotificationQueues.size();
    pvNotificationQueues.push_back(this->wakeUpQueue);
    // Now we can build a future queue that is notified whenever one of the PVs'
    // future queues has a new element.
    notificationQueue = cppext::when_any(
            pvNotificationQueues.begin(), pvNotificationQueues.end());
  }
  // TODO Document
  // When we receive a notification for a PV for which the last notification has
  // not been completely handled yet, we have to receive the updated value at a
  // later point in time. We must not receive more updates than we have been
  // notified of. Otherwise there would be notifications inside the notification
  // queue for value updates that have already been processed and the
  // notification queue could run out of space if more updates are pushed before
  // the notifications have been removed from the notification queue.
  // Therefore, we keep track of how many notifications we received for each PV
  // that have not been processed yet and only receive that number of value
  // updates.
  std::vector<std::size_t> numberOfPendingUpdatesForPV(wakeUpQueueIndex, 0);
  // We cannot check the abort condition here because we have to hold a lock on
  // the mutex while checking the condition.
  while (true) {
    std::forward_list<std::function<void()>> notifyFunctions;
    size_t pvIndex;
    // We have to call pop_wait BEFORE acquiring the mutex. Otherwise, we would
    // block the mutex while waiting and the thread could never be woken up,
    // because the code sending the wake-up request has to acquire the mutext as
    // well.
    notificationQueue.pop_wait(pvIndex);
    // We limit the code where we hold the mutex to the part where it is really
    // needed. In particular, we do not want to hold the lock when calling
    // notification callbacks as this could result in a deadlock in the worst
    // case.
    {
      std::unique_lock<std::recursive_mutex> lock(this->mutex);
      // If a shutdown has been requested, we quit immediately.
      if (this->notificationThreadShutdownRequested) {
        return;
      }
      while (!this->pendingCallNotify.empty()) {
        std::shared_ptr<ControlSystemAdapterSharedPVSupportBase> sharedPVSupport(
            std::move(this->pendingCallNotify.front()));
        this->pendingCallNotify.pop_front();
        // Please note that we do not use the variable pvIndex here. The value
        // of that variable must be preserved because we use it later when
        // processing notifications from the notificationQueue.
        std::size_t pendingPVIndex = sharedPVSupport->getIndex();
        if (sharedPVSupport->readyForNextNotification()
            && numberOfPendingUpdatesForPV[pendingPVIndex]) {
          --numberOfPendingUpdatesForPV[pendingPVIndex];
          try {
            if (ChimeraTK::detail::getFutureQueueFromTransferFuture(
                    pvsForNotification[pendingPVIndex]->readAsync()).pop()) {
              // Whenever pop() was successful, we also have to call postRead().
              pvsForNotification[pendingPVIndex]->postRead();
              auto notifyFunction = sharedPVSupport->doNotify();
              if (notifyFunction) {
                notifyFunctions.push_front(std::move(notifyFunction));
              }
            } else {
              // If there are less value updates than notifications, something
              // is seriously wrong.
              assert(false);
            }
          } catch (ChimeraTK::detail::DiscardValueException &) {
            // We can simply ignore such an exception - it only means that we
            // did not actually receive a new value - so we do not have to
            // notify the PV support.
          }
        }
      }
      // We limit the number of events that we process before actually calling
      // the callbacks. In theory, it is possible that events arrive faster than
      // we can process them. If we did not have a limit, the list would grow
      // and grow until we run out of memory.
      int numberOfPVsWithEvent = 0;
      // We do at least one iteration because we have at least one pvIndex (from
      // calling notificationQueue.pop_wait() above).
      do {
        // If we find a request to wake up this thread, we break the loop right
        // now. We use >= instead of == so that we never use an invalid index
        // into one of the arrays. This should never happen unless there is a
        // bug in the future queue, but it is better to be safe than sorry.
        if (pvIndex >= wakeUpQueueIndex) {
          this->wakeUpQueue.pop();
          break;
        }
        ++numberOfPVsWithEvent;
        auto sharedPVSupport = this->sharedPVSupportsByIndex[pvIndex].lock();
        // If the notification is for a PV support that does not exist we ignore
        // it, but we still make sure that we receive the value update. This
        // way, a PV support will have the newest value if it is created at a
        // later point in time.
        if (!sharedPVSupport) {
          try {
            if (ChimeraTK::detail::getFutureQueueFromTransferFuture(
                    pvsForNotification[pvIndex]->readAsync()).pop()) {
              // Whenever pop() was successful, we also have to call postRead().
              pvsForNotification[pvIndex]->postRead();
            }
          } catch (ChimeraTK::detail::DiscardValueException &) {
            // We can simply ignore such an exception - it only means that we
            // did not actually receive a new value.
          }
          continue;
        }
        if (sharedPVSupport->readyForNextNotification()) {
          try {
            if (ChimeraTK::detail::getFutureQueueFromTransferFuture(
                    pvsForNotification[pvIndex]->readAsync()).pop()) {
              // Whenever pop() was successful, we also have to call postRead().
              pvsForNotification[pvIndex]->postRead();
              auto notifyFunction = sharedPVSupport->doNotify();
              if (notifyFunction) {
                notifyFunctions.push_front(std::move(notifyFunction));
              }
            } else {
              // If there are less value updates than notifications, something
              // is seriously wrong.
              assert(false);
            }
          } catch (ChimeraTK::detail::DiscardValueException &) {
            // We can simply ignore such an exception - it only means that we
            // did not actually receive a new value - so we do not have to
            // notify the PV support.
          }
        } else {
          // We received a notification for a value update, but we are not ready
          // to process this update yet. We have to remember that we received a
          // notification so that we can receive the updated value as soon as we
          // are ready.
          ++numberOfPendingUpdatesForPV[pvIndex];
        }
      } while (numberOfPVsWithEvent < 1000 && notificationQueue.pop(pvIndex));
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
  this->wakeUpNotificationThread();
}

void ControlSystemAdapterPVProvider::wakeUpNotificationThread() {
  // This method is only called while already holding a lock on the mutex.
  this->wakeUpQueue.push();
}

} // namespace EPICS
} // namespace ChimeraTK
