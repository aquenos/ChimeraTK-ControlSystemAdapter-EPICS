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

#ifndef CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_SHARED_PV_SUPPORT_IMPL_H
#define CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_SHARED_PV_SUPPORT_IMPL_H

#include <exception>
#include <iterator>

#include "errorPrint.h"

#include "ControlSystemAdapterSharedPVSupportDef.h"

// We include the implementation of the ControlSystemAdapterPVSupport because
// that implementation needs to be available when using this implementation.
// We do this AFTER including the definition of
// ControlSystemAdapterSharedPVSupport because the implementation of
// ControlSystemAdapterPVSupport depends on this definition.
#include "ControlSystemAdapterPVSupportImpl.h"

namespace ChimeraTK {
namespace EPICS {

template<typename T>
ControlSystemAdapterSharedPVSupport<T>::ControlSystemAdapterSharedPVSupport(
    ControlSystemAdapterPVProvider::SharedPtr const &pvProvider,
    std::string const &name)
    : alreadyReadNewValue(false), initialValueAvailable(true),
      mutex(pvProvider->mutex), name(name), notificationPendingCount(0),
      notifyCallbackCount(0), pvProvider(pvProvider) {
  {
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    this->processArray = this->pvProvider->pvManager
      ->template getProcessArray<T>(name);
  }
}

template<typename T>
bool ControlSystemAdapterSharedPVSupport<T>::canRead() {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  return this->processArray->isReadable();
}

template<typename T>
bool ControlSystemAdapterSharedPVSupport<T>::canWrite() {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  return this->processArray->isWriteable();
}

template<typename T>
std::size_t ControlSystemAdapterSharedPVSupport<T>::getNumberOfElements() {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  return this->processArray->getNumberOfSamples();
}

template<typename T>
std::tuple<typename PVSupport<T>::Value, VersionNumber> ControlSystemAdapterSharedPVSupport<T>::initialValue() {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  if (!this->initialValueAvailable) {
    throw std::runtime_error("The initial value is not available any longer.");
  }
  return std::make_tuple(
    this->processArray->accessChannel(0),
    this->processArray->getVersionNumber());
}

template<typename T>
std::shared_ptr<ControlSystemAdapterPVSupport<T>> ControlSystemAdapterSharedPVSupport<T>::createPVSupport() {
  auto instance = std::make_shared<ControlSystemAdapterPVSupport<T>>(
    this->shared_from_this());
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  this->pvSupports.push_front(instance);
  return instance;
}

template<typename T>
bool ControlSystemAdapterSharedPVSupport<T>::read(
    ReadCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  decltype(this->lastValueRead) lastValue;
  VersionNumber lastVersionNumber;
  try {
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    // If the last notification has finished, we can try to read a new value.
    // Otherwise, we have to reuse the last one.
    if (this->notificationPendingCount == 0 && !this->alreadyReadNewValue) {
      // By calling readNonBlocking(), we make the next value available. If
      // there is a newer value, readNonBlocking() returns true and we get that
      // value. Otherwise we reuse the last value.
      // If we have never read a value, but no (new) value is available yet, we
      // use the initial value.
      if (this->processArray->readNonBlocking() || !this->lastValueRead) {
        this->initialValueAvailable = false;
        // We have to remember that we already read a new value. Otherwise, we
        // would overwrite this value when doNotify() or this method are called
        // again.
        this->alreadyReadNewValue = true;
        // The logic here is the same as in doNotify().
        auto newValue = std::make_shared<std::vector<T>>(
          this->processArray->getNumberOfSamples());
        std::swap(*newValue, this->processArray->accessChannel(0));
        this->lastValueRead = newValue;
        this->lastVersionNumberRead = this->processArray->getVersionNumber();
        this->pvProvider->scheduleCallNotify(this->shared_from_this());
      }
    }
    lastValue = this->lastValueRead;
    lastVersionNumber = this->lastVersionNumberRead;

  } catch (...) {
    if (errorCallback) {
      errorCallback(true, std::current_exception());
    }
    // Handling is complete, so we return true.
    return true;
  }
  if (successCallback) {
    successCallback(true, lastValue, lastVersionNumber);
  }
  return true;
}

template<typename T>
bool ControlSystemAdapterSharedPVSupport<T>::write(
    Value const &value,
    WriteCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  // The write method that takes an rvalue swaps values, so creating a copy of
  // the original vectory here and then passing this copy is only slightly less
  // efficient than actually copying to the destination vector, but it
  // simplifies the code significantly.
  Value valueCopy(value);
  return this->write(std::move(valueCopy), successCallback, errorCallback);
}

template<typename T>
bool ControlSystemAdapterSharedPVSupport<T>::write(
    Value &&value,
    WriteCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  try {
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    if (!this->processArray->isWriteable()) {
      throw std::logic_error("This process variable is not writable.");
    }
    auto &destination = this->processArray->accessChannel(0);
    std::swap(destination, value);
    this->initialValueAvailable = false;
    this->processArray->write();
  } catch (...) {
    if (errorCallback) {
      errorCallback(true, std::current_exception());
    }
    // Handling is complete, so we return true.
    return true;
  }
  // It is important that we notify the callback AFTER releasing the lock.
  // Otherwise, we might risk a deadlock, depending on what the callback does.
  if (successCallback) {
    successCallback(true);
  }
  return true;
}

template<typename T>
std::function<void()> ControlSystemAdapterSharedPVSupport<T>::doNotify() {
  // This method is only called while holding a lock on the mutex, so we do not
  // have to acquire a lock here.
  // This method should only be called if notificationPendingCount is zero.
  // We only use an assertion here because this method is only called by code
  // that is within the control of this module.
  assert(notificationPendingCount == 0);
  // If there are no notify callbacks, we do not read the next value. This has
  // the effect that the next value can be "pulled" from the FIFO by calling
  // read(...). If we read the value here, code repeatedly calling read(...)
  // might miss values.
  if (this->notifyCallbackCount == 0) {
    this->alreadyReadNewValue = false;
    return std::function<void()>();
  }
  // By calling readNonBlocking(), we make the next value available. If there
  // is no newer value, readNonBlocking() returns false and we do not have to
  // notify anyone.
  // We only call readNonBlocking() if we do not already have a new value (that
  // has been read by the read(...) method).
  if (!this->alreadyReadNewValue && !this->processArray->readNonBlocking()) {
    return std::function<void()>();
  }
  this->alreadyReadNewValue = false;
  this->initialValueAvailable = false;
  // We are going to swap the vectors because this is more efficient than
  // copying (in particular if the vectors have many elements). We have to
  // make our vector the same size as the vector used by the ProcessArray, or
  // we will get an exception on the next read attempt.
  auto newValue = std::make_shared<std::vector<T>>(
    this->processArray->getNumberOfSamples());
  std::swap(*newValue, this->processArray->accessChannel(0));
  this->lastValueRead = newValue;
  this->lastVersionNumberRead = this->processArray->getVersionNumber();
  // We have to make a copy of the list of PV supports because we want to
  // release the mutex before actually notifying them. While creating this
  // copy, we convert from weak to shared pointers and remove weak pointers
  // that do not refer to live objects.
  int numberOfPvSupports = 0;
  std::forward_list<std::shared_ptr<ControlSystemAdapterPVSupport<T>>> pvSupports;
  for (auto i = this->pvSupports.before_begin(); std::next(i) != this->pvSupports.end();) {
    auto sharedPtr = std::next(i)->lock();
    if (sharedPtr) {
      pvSupports.emplace_front(std::move(sharedPtr));
      ++i;
      ++numberOfPvSupports;
    } else {
      this->pvSupports.erase_after(i);
      // After erasing the following element, we do not increment the iterator
      // because the following element is already the next element.
    }
  }
  this->notificationPendingCount += numberOfPvSupports;
  std::forward_list<NotifyCallback> callbacks;
  for (auto &pvSupport : pvSupports) {
    auto callback = pvSupport->getNotifyCallback();
    if (callback) {
      callbacks.push_front(std::move(callback));
      // We have to set the PV support's notification pending flag so that it
      // will decrement the notificationPendingCount when its notifyFinished
      // method is called.
      pvSupport->notificationPending = true;
    } else {
      --this->notificationPendingCount;
    }
  }
  auto &value = this->lastValueRead;
  auto &versionNumber = this->lastVersionNumberRead;
  return [value, versionNumber, callbacks = std::move(callbacks)](){
      for (auto &callback : callbacks) {
        try {
          callback(value, versionNumber);
          return;
        } catch (std::exception &e) {
          errorPrintf(
            "A notification callback threw an exception. This indicates a bug in the record device support code. The exception message was: %s",
            e.what());
        } catch (...) {
          errorPrintf(
            "A notification callback threw an exception. This indicates a bug in the record device support code.");
        }
      }
    };
}

template<typename T>
bool ControlSystemAdapterSharedPVSupport<T>::readyForNextNotification() {
  // We are ready to deliver the next notification when the last notifications
  // that we sent have all been processed.
  // This method is only called while holding a lock on the mutex, so we do not
  // have to acquire a lock here.
  return this->notificationPendingCount == 0;
}

template<typename T>
void ControlSystemAdapterSharedPVSupport<T>::notifyFinished() {
  // The code calling this method already acquires a lock on the shared mutex.
  --this->notificationPendingCount;
  // If the count is now zero, we have to make sure that doNotify() is called
  // again because there might be more values waiting to be read.
  if (notificationPendingCount == 0) {
    this->pvProvider->scheduleCallNotify(this->shared_from_this());
  }
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_SHARED_PV_SUPPORT_IMPL_H
