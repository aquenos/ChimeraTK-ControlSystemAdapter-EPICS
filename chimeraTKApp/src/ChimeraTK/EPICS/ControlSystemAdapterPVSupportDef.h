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

#ifndef CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_SUPPORT_DEF_H
#define CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_SUPPORT_DEF_H

#include <mutex>

#include "PVSupport.h"

#include "ControlSystemAdapterPVSupportFwdDecl.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * PVSupport implementation used by the ControlSystemAdapterPVProvider.
 *
 * This class delegates almost all work to ControlSystemAdapterSharedPVSupport.
 * It only exists so that we can distinguish different notification callback
 * registrations.
 */
template<typename T>
class ControlSystemAdapterPVSupport : public PVSupport<T> {

public:

  /**
   * Type of the callback function that is called in case of an error.
   */
  using ErrorCallback = typename PVSupport<T>::ErrorCallback;

  /**
   * Type of the callback passed to notify(...).
   */
  using NotifyCallback = typename PVSupport<T>::NotifyCallback;

  /**
   * Type of the error callback passed to notify(...).
   */
  using NotifyErrorCallback = typename PVSupport<T>::NotifyErrorCallback;

  /**
   * Type of the callback passed to read(...).
   */
  using ReadCallback = typename PVSupport<T>::ReadCallback;

  /**
   * Type of a shared value vector (that is the type of value passed to a
   * NotifyCallback).
   */
  using SharedValue = typename PVSupport<T>::SharedValue;

  /**
   * Type of a value vector.
   */
  using Value = typename PVSupport<T>::Value;

  /**
   * Type of the callback passed to write(...).
   */
  using WriteCallback = typename PVSupport<T>::WriteCallback;

  /**
   * Creates a new PV support for the specified shared instance.
   *
   * This constructor does not take care of registering this instance with the
   * shared instance. The calling code has to do this in order for notifications
   * to work correctly.
   */
  ControlSystemAdapterPVSupport(
      std::shared_ptr<ControlSystemAdapterSharedPVSupport<T>> shared);

  /**
   * Destroys this instance. This decrements the counter for pending
   * notifications in the shared instance when a notification callback has been
   * called, but has not called notifyFinished() yet.
   */
  virtual ~ControlSystemAdapterPVSupport() noexcept;

  // Declared in PVSupport.
  virtual bool canNotify() override;

  // Declared in PVSupport.
  virtual bool canRead() override;

  // Declared in PVSupport.
  virtual bool canWrite() override;

  // Declared in PVSupport.
  virtual std::size_t getNumberOfElements() override;

  // Declared in PVSupport.
  virtual std::tuple<Value, TimeStamp, VersionNumber> initialValue() override;

  // Declared in PVSupport.
  virtual void notify(
      NotifyCallback const &successCallback,
      NotifyErrorCallback const &errorCallback) override;

  // Declared in PVSupport.
  virtual void notifyFinished() override;

  // Declared in PVSupport.
  virtual bool read(
      ReadCallback const &successCallback,
      ErrorCallback const &errorCallback) override;

  // Declared in PVSupport.
  virtual bool write(Value const &value,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) override;

  // Declared in PVSupport.
  virtual bool write(Value &&value,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) override;

private:

  /**
   * The ControlSystemAdapterPVSupport and ControlSystemAdapterSharedPVSupport
   * classes are closely related to each other, so they have to be friends in
   * order to access their respective private fields and methods.
   */
  friend class ControlSystemAdapterSharedPVSupport<T>;

  /**
   * Reference to the mutex of the shared instance. Keeping such a reference
   * locally is safe because we also keep a reference to the shared instance via
   * a shared pointer. This means that this reference is valid as long as this
   * instance exists.
   */
  std::recursive_mutex &mutex;

  /**
   * Indicates whether the notification callback has been called, but
   * notifyFinished() has not been called yet. We internally use this flag in
   * order to determine when we have to decrement the shared instance's
   * notification pending counter.
   */
  bool notificationPending;

  /**
   * Function that is called when a new value is available. May be null if no
   * callback has been registered.
   */
  NotifyCallback notifyCallback;

  /**
   * Pointer to the shared instance. This pointer is initialized during
   * construction and kept alive as long as this object exists.
   */
  std::shared_ptr<ControlSystemAdapterSharedPVSupport<T>> const shared;

  /**
   * Returns the notification callback for this PV support.
   *
   * This method must only be called while holding a lock on the mutex.
   */
  NotifyCallback getNotifyCallback();

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_SUPPORT_DEF_H
