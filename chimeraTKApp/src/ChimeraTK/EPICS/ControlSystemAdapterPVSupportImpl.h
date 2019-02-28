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

#ifndef CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_SUPPORT_IMPL_H
#define CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_SUPPORT_IMPL_H

#include <exception>

#include "errorPrint.h"

#include "ControlSystemAdapterPVSupportDef.h"

// We include the implementation of the ControlSystemAdapterSharedPVSupport
// because that implementation needs to be available when using this
// implementation.
// We do this AFTER including the definition of ControlSystemAdapterPVSupport
// because the implementation of ControlSystemAdapterSharedPVSupport depends on
// this definition.
#include "ControlSystemAdapterSharedPVSupportImpl.h"

namespace ChimeraTK {
namespace EPICS {

template<typename T>
ControlSystemAdapterPVSupport<T>::ControlSystemAdapterPVSupport(
    std::shared_ptr<ControlSystemAdapterSharedPVSupport<T>> shared)
    : mutex(shared->mutex), notificationPending(false), shared(shared) {
}

template<typename T>
ControlSystemAdapterPVSupport<T>::~ControlSystemAdapterPVSupport() noexcept {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  if (this->notificationPending) {
    this->shared->notifyFinished();
    this->notificationPending = false;
  }
  // When this object is destroyed and a notify callback is registered, we have
  // to decrement the counter counting the number of callbacks because this
  // callback is not active any longer.
  if (this->notifyCallback) {
    --this->shared->notifyCallbackCount;
    this->notifyCallback = NotifyCallback();
  }
}

template<typename T>
bool ControlSystemAdapterPVSupport<T>::canNotify() {
  return this->shared->canNotify();
}

template<typename T>
bool ControlSystemAdapterPVSupport<T>::canRead() {
  return this->shared->canRead();
}

template<typename T>
bool ControlSystemAdapterPVSupport<T>::canWrite() {
  return this->shared->canWrite();
}

template<typename T>
std::size_t ControlSystemAdapterPVSupport<T>::getNumberOfElements() {
  return this->shared->getNumberOfElements();
}

template<typename T>
std::tuple<typename PVSupport<T>::Value, VersionNumber> ControlSystemAdapterPVSupport<T>::initialValue() {
  return this->shared->initialValue();
}

template<typename T>
void ControlSystemAdapterPVSupport<T>::notify(
    NotifyCallback const &successCallback,
    NotifyErrorCallback const &errorCallback) {
  if (!this->canNotify()) {
    throw std::logic_error(
      "This process variable does not support change notifications because it is not readable.");
  }
  {
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    // When a callback is registered and there was no callback registered
    // before, we have to increment the number of registered callbacks.
    // Conversely, when there was a callback registered before and we remove
    // this callback now, we have to decrement the count.
    if (!this->notifyCallback && successCallback) {
      ++this->shared->notifyCallbackCount;
    } else if (this->notifyCallback && !successCallback) {
      --this->shared->notifyCallbackCount;
    }
    this->notifyCallback = successCallback;
    // When notifications are cancelled after the notification callback has been
    // called, but before notifyFinished has been called, we want to reset the
    // notification pending flag. After cancelling notifications, further events
    // will not result in notifications anyway, so there is no sense in blocking
    // the delivery for other PVSupport instances. In addition to that, this
    // makes sure that the notification pending flag is reset, even if the code
    // that originally registered notifications does not take care of calling
    // notifyFinished after calling cancelNotify().
    if (!successCallback && this->notificationPending) {
      this->shared->notifyFinished();
      this->notificationPending = false;
    }
    // We want the callback to be notified with the current value. If we did not
    // do this, there might be no notification for a very long time (if the
    // value did not change) and the record might potentially have an old value
    // for that time.
    // We have to call doInitialNotification while holding the mutex, but the
    // callback that we pass will be called from the notification thread,
    // without holding the mutex, so there is no risk of a dead-lock.
    if (successCallback && !this->notificationPending) {
      this->shared->doInitialNotification(successCallback);
      this->notificationPending = true;
    }
  }
}

template<typename T>
void ControlSystemAdapterPVSupport<T>::notifyFinished() {
  std::lock_guard<std::recursive_mutex> lock(this->mutex);
  // We check whether notificationPending is actually set. We reset that flag
  // when notifications are cancelled, but this method might be called after
  // they have been cancelled. If we did not check the flag, we would decrement
  // the counter twice.
  if (this->notificationPending) {
    this->shared->notifyFinished();
    this->notificationPending = false;
  }
}

template<typename T>
bool ControlSystemAdapterPVSupport<T>::read(
    ReadCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  return this->shared->read(successCallback, errorCallback);
}

template<typename T>
bool ControlSystemAdapterPVSupport<T>::write(
    Value const &value,
    VersionNumber const &versionNumber,
    WriteCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  return this->shared->write(
      value, versionNumber, successCallback, errorCallback);
}

template<typename T>
bool ControlSystemAdapterPVSupport<T>::write(
    Value &&value,
    VersionNumber const &versionNumber,
    WriteCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  return this->shared->write(
      value, versionNumber, successCallback, errorCallback);
}

template<typename T>
typename ControlSystemAdapterPVSupport<T>::NotifyCallback ControlSystemAdapterPVSupport<T>::getNotifyCallback() {
  return this->notifyCallback;
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_SUPPORT_IMPL_H
