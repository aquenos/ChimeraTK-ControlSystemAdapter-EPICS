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

#include <ChimeraTK/ReadAnyGroup.h>
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
      pvManager(pvManager) {
  this->insertCreatePVSupportFunc<std::int8_t>();
  this->insertCreatePVSupportFunc<std::uint8_t>();
  this->insertCreatePVSupportFunc<std::int16_t>();
  this->insertCreatePVSupportFunc<std::uint16_t>();
  this->insertCreatePVSupportFunc<std::int32_t>();
  this->insertCreatePVSupportFunc<std::uint32_t>();
  this->insertCreatePVSupportFunc<std::int64_t>();
  this->insertCreatePVSupportFunc<std::uint64_t>();
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
  // We also have to add the special PV that is used for waking up the
  // notification thread. We include the receiver side because that is the one
  // that will trigger the notifications.
  auto wakeUpPVSenderAndReceiver = createSynchronizedProcessArray<int>(1);
  this->wakeUpPV = wakeUpPVSenderAndReceiver.first;
  this->pvsForNotification.push_back(wakeUpPVSenderAndReceiver.second);
  // We will never create a PV support for our internal wake-up PV, so the
  // max. number of PV supports is one less than the number of PVs. However, by
  // choosing the vector size the same, we can use any index for which we
  // receive a notification without having to check first whether it is a
  // regular PV or the wake-up PV first.
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
    std::type_info const &elementType) {
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

void ControlSystemAdapterPVProvider::runInNotificationThread(
    std::function<void()> const &task) {
  // This method is only called while already holding a lock on the mutex.
  if (this->notificationThreadShutdownRequested) {
    throw std::runtime_error(
      "Tasks cannot be submitted because this PV provider is being destroyed.");
  }
  this->tasks.push(task);
  this->wakeUpNotificationThread();
}

void ControlSystemAdapterPVProvider::runNotificationThread() {
  try {
    // We create a read-any group that will allow us to wait for any of the PVs
    // (supporting notifications) to receive an update notification. This list
    // of PVs includes our special wake-up PV that we use to wake up this thread
    // while it is waiting for a notification.
    ReadAnyGroup notificationGroup(
        this->pvsForNotification.begin(), this->pvsForNotification.end());
    // We cannot check the abort condition here because we have to hold a lock on
    // the mutex while checking the condition.
    while (true) {
      ReadAnyGroup::Notification notification;
      // We have to call waitAny before acquiring the mutex. Otherwise, we would
      // block the mutex while waiting and the thread could never be woken up,
      // because the code sending the wake-up request has to acquire the mutex
      // as well.
      notification = notificationGroup.waitAny();
      // We limit the code where we hold the mutex to the part where it is
      // really needed. In particular, we do not want to hold the lock when
      // calling notification callbacks as this could result in a deadlock in
      // the worst case.
      std::function<void()> notifyFunction;
      {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        // If there are any notification tasks, we execute them now.
        while (!this->tasks.empty()) {
          auto task = std::move(this->tasks.front());
          this->tasks.pop();
          // We do not want to hold the lock on the mutex while executing the
          // task.
          lock.unlock();
          task();
          lock.lock();
        }
        // If a shutdown has been requested, we quit immediately.
        if (this->notificationThreadShutdownRequested) {
          return;
        }
        auto sharedPVSupport =
            this->sharedPVSupportsByIndex[notification.getIndex()].lock();
        if (sharedPVSupport) {
          // We cannot process the notification if an earlier notification for
          // the same PV is still being processed. In this case we sleep until
          // this thread is woken up and then check again. When a PV support is
          // finished with the notification process, it will wake up this
          // thread, so we should eventually wake up and find that we can
          // process the notification.
          while (!sharedPVSupport->readyForNextNotification()) {
            this->notificationThreadCv.wait(lock);
            // If there are any notification tasks, we execute them now. We have
            // to do this here because we might sleep again if the PV support is
            // still not ready for the next notification.
            while (!this->tasks.empty()) {
              auto task = std::move(this->tasks.front());
              this->tasks.pop();
              // We do not want to hold the lock on the mutex while executing
              // the task.
              lock.unlock();
              task();
              lock.lock();
            }
            // If a shutdown has been requested, we quit immediately.
            if (this->notificationThreadShutdownRequested) {
              return;
            }
          }
          // We know that at that point, the task queue is empty. Tasks are only
          // added while holding a lock on the mutex and we checked that the
          // queue is empty after acquiring the mutex.
          // This is important because we use the task queue to notify callbacks
          // with the current value and the same callback will only be called
          // when there actually is a new value (that we might accept right in
          // the next line).
          if (notification.accept()) {
            notifyFunction = sharedPVSupport->doNotify();
          }
        } else {
          // If the notification is for a PV for which there is no PV support
          // (yet), we can simply accept it. Note that this would also happen
          // through the destructor of the notification object, but doing it
          // explicitly looks cleaner.
          notification.accept();
        }
      }
      // After releasing the lock, we call the notify function. It is important
      // that we do not do this while holding the lock because we would risk a
      // deadlock.
      if (notifyFunction) {
        notifyFunction();
      }
    }
  } catch (boost::thread_interrupted &) {
    return;
  }
}

void ControlSystemAdapterPVProvider::wakeUpNotificationThread() {
  // This method is only called while already holding a lock on the mutex.
  this->wakeUpPV->write();
  this->notificationThreadCv.notify_all();
}

} // namespace EPICS
} // namespace ChimeraTK
