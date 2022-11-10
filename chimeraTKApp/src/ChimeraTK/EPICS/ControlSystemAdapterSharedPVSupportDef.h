/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2018-2022 aquenos GmbH
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

#ifndef CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_SHARED_PV_SUPPORT_DEF_H
#define CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_SHARED_PV_SUPPORT_DEF_H

#include <cstdint>
#include <forward_list>
#include <memory>

#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/ControlSystemAdapter/ProcessArray.h>

#include "ControlSystemAdapterPVProvider.h"
#include "ControlSystemAdapterPVSupportFwdDecl.h"
#include "PVSupport.h"

#include "ControlSystemAdapterSharedPVSupportFwdDecl.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * Base interface for all variants of ControlSystemAdapterSharedPVSupport.
 *
 * This interface defines the methods that have to be called by the
 * ControlSystemAdapterPVProvider and that to do not depend the element type of
 * the PV.
 */
class ControlSystemAdapterSharedPVSupportBase {

protected:

  /**
   * The ControlSystemAdapterPVProvider is a friend so that it can call the
   * doNotify() and readyForNextNotification() methods.
   */
  friend class ControlSystemAdapterPVProvider;

  /**
   * Constructor. Sets the index to the specified number.
   */
  ControlSystemAdapterSharedPVSupportBase(std::size_t index) : index(index) {
  }

  /**
   * Destructor. The destructor is protected because an instance should never be
   * destroyed through a pointer to this interface.
   */
  virtual ~ControlSystemAdapterSharedPVSupportBase() noexcept {
  }

  /**
   * Triggers notification of registered callbacks. This is called by the
   * PVProvider when it determines that a new value might be available for the
   * PV or when it has explicitly been asked to do so.
   *
   * This method must only be called while holding a lock on the mutex.
   */
  virtual std::function<void()> doNotify() = 0;

  /**
   * Returns the index that is internally assigned to this PV by the PV
   * provider. This is primarily used by the PV provider to quickly find related
   * data structures for a given PV support.
   *
   * Note that this index might be undefined if the PV does not support value
   * update notifications, so it should not be used by any code outside the PV
   * provider.
   */
  inline std::size_t getIndex() const {
    return this->index;
  }

  /**
   * Calls the underlying ProcessArray’s write() method, but only if willWrite()
   * has not been called.
   *
   * This ensures that every process variable is written once in the
   * initialization phase, as required by the ApplicationCore specification.
   */
  virtual void initialWriteIfNeeded() = 0;

  /**
   * Tells whether doNotify() may be called. The PVProvider will only call
   * doNotify() when this method returns true. Otherwise, it will periodically
   * call this method again until it returns false.
   *
   * This method must only be called while holding a lock on the mutex.
   */
  virtual bool readyForNextNotification() = 0;

private:

  /**
   * Index that is internally assigned to this PV by the PV provider. This index
   * is primarily used by the PV provider to quickly find related data
   * structures for a given PV support.
   */
  std::size_t index;

};

/**
 * Shared state for ControlSystemAdapterPVSupport.
 *
 * All instances of ControlSystemAdapterPVSupport that are created for the same
 * process variable internally point to the same instance of this class. This
 * makes it possible to coordinate reading data in a way so that all PVSupport
 * instances see all events.
 *
 * When a PVSupport is first requested for a process variable, the
 * ControlSystemAdapterPVProvider creates an instance of this class and then
 * reuses it when more PVSupport instances for the same process variable are
 * requested. These instances are created by calling the createPVSupport()
 * method.
 *
 * The template parameter T is the element type of the ProcessArray for the
 * process variable.
 */
template<typename T>
class ControlSystemAdapterSharedPVSupport
    : public ControlSystemAdapterSharedPVSupportBase,
      public std::enable_shared_from_this<ControlSystemAdapterSharedPVSupport<T>> {

public:

  /**
   * Type of the callback that is called in case of an error.
   */
  using ErrorCallback = typename PVSupport<T>::ErrorCallback;

  /**
   * Type of the callback passed to read(...).
   */
  using ReadCallback = typename PVSupport<T>::ReadCallback;

  /**
   * Type of the callback passed to notify(...).
   */
  using NotifyCallback = typename PVSupport<T>::NotifyCallback;

  /**
   * Type of a value vector.
   */
  using Value = typename PVSupport<T>::Value;

  /**
   * Type of the callback passed to write(...).
   */
  using WriteCallback = typename PVSupport<T>::WriteCallback;

  /**
   * Creates a shared PV support instance for the specified process variable
   * name. The pointer to the PV provider must point to the PV provider that
   * is creating this instance. The index is the internal index used by the PV
   * provider to identify the PV corresponding to this PV support.
   */
  ControlSystemAdapterSharedPVSupport(
      ControlSystemAdapterPVProvider::SharedPtr const &pvProvider,
      std::string const &name, std::size_t index);

  /**
   * Tells whether the process variable represented by this PV support supports
   * notifications. A PV that supports notifications can also be read.
   */
  bool canNotify();

  /**
   * Tells whether the process variable represented by this PV support can be
   * read.
   */
  bool canRead();

  /**
   * Tells whether the process variable represented by this PV support can be
   * written.
   */
  bool canWrite();

  /**
   * Creates a new PVSupport instance that is linked to this shared instance.
   * The ControlSystemAdapterPVProvider calls this method every time a
   * PVSupport is requested for the PV represented by this PV support.
   */
  std::shared_ptr<ControlSystemAdapterPVSupport<T>> createPVSupport();

  /**
   * Returns the number of elements each value of the process variable has.
   */
  std::size_t getNumberOfElements();

  /**
   * Returns the current value and version number of the underlying
   * ProcessArray. As long as no read or write operations have happened and no
   * notifications have been received, this will be the initial value of the
   * underlying ProcessArray. Otherwise, it will be the value that was read,
   * written, or received by the last operation.
   */
  std::tuple<Value, VersionNumber> initialValue();

  /**
   * Reads the next available value and calls the callback. if there are PV
   * supports linked to this shared instance that deliver notifications, this
   * method might not actually read a new value, but might simply call the
   * callback with the last known value. This only happens when the
   * notifications have not finished since reading the last value.
   *
   * The callback is always called before this method returns.
   */
  bool read(
      ReadCallback const &successCallback,
      ErrorCallback const &errroCallback);

  /**
   * Called to indicate that the process variable is going to be written during
   * the startup phase.
   *
   * We use this in initialWriteIfNeeded() in order to decide whether we need to
   * write the underlying process variable once.
   */
  void willWrite();

  /**
   * Writes a value and calls the callback.
   *
   * The callback is always called before this method returns.
   */
  bool write(
      Value const &value,
      VersionNumber const &versionNumber,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback);

  /**
   * Writes a value and calls the callback.
   *
   * The callback is always called before this method returns.
   */
  bool write(
      Value &&value,
      VersionNumber const &versionNumber,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback);

protected:

  // Declared in ControlSystemAdapterSharedPVSupportBase.
  virtual std::function<void()> doNotify() override;

  // Declared in ControlSystemAdapterSharedPVSupportBase.
  virtual void initialWriteIfNeeded() override;

  // Declared in ControlSystemAdapterSharedPVSupportBase.
  virtual bool readyForNextNotification() override;

private:

  /**
   * The ControlSystemAdapterPVSupport and ControlSystemAdapterSharedPVSupport
   * classes are closely related to each other, so they have to be friends in
   * order to access their respective private fields and methods.
   */
  friend class ControlSystemAdapterPVSupport<T>;

  /**
   * Last value that his been read or written. This value might have been read by the
   * doNotify() or the read(...) method or written by the write(...) method.
   */
  std::shared_ptr<Value const> lastValue;

  /**
   * Version number / time stamp belonging to the value stored in lastValue.
   */
  VersionNumber lastVersionNumber;

  /**
   * Mutex for which a lock is acquired when modifying shared state. This is the
   * same mutex that is used by the ControlSystemAdapterPVProvider. As we retain
   * a reference to the PVProvider as long as this instance exists, it is safe
   * to locally keep a reference to that mutex.
   */
  std::recursive_mutex &mutex;

  /**
   * Name of the process variable for which this shared PV support has been
   * created.
   */
  std::string name;

  /**
   * Counter for the number of notifications that are in progress. This counter
   * is set by doNotify() before calling the doNotify(...) methods of the PV
   * supports that share this instance. These methods in turn call
   * notifyFinished() (either directly or indirectly at a later point in time),
   * so that this counter gets decremented again.
   *
   * Effectively, this counter is zero when all calls to doNotify(...) have
   * finished their work and we may thus read the next value.
   */
  int notificationPendingCount;

  /**
   * Counter for the number of notify callbacks that are currently registered.
   * This counter is updated by the ControlSystemAdapterPVSupport when a notify
   * callback is registered or unregistered.
   */
  int notifyCallbackCount;

  /**
   * ProcessArray for the process variable represented by this instance. The
   * ProcessArray is internally used for reading and writing values.
   */
  typename ProcessArray<T>::SharedPtr processArray;

  /**
   * Pointer to the PV provider that created this instance.
   */
  ControlSystemAdapterPVProvider::SharedPtr pvProvider;

  /**
   * PV supports that have been created by createPVSupport(). We only keep weak
   * references to these PV supports. This way, we can deliver notifications to
   * each PV support that is alive, but we do not keep instances alive that are
   * not in use any longer.
   */
  std::forward_list<std::weak_ptr<ControlSystemAdapterPVSupport<T>>> pvSupports;

  /**
   * Flag indicating whether at least one of the records that uses this PV
   * support is going to call write() during the initialization phase of the
   * IOC.
   */
  bool willWriteCalled;

  /**
   * Calls the specified notify callback with the current value of the PV. This
   * is guaranteed to happen before notifying it with a regular notification.
   * The callback must still take care of calling notifyFinished().
   *
   * This method must only be called while holding a lock on the mutex. It is
   * intended for use by the ControlSystemAdapterPVSupport.
   */
  void doInitialNotification(NotifyCallback const &callback);

  /**
   * Called by each ControlSystemAdapterPVSupport when it has finished the
   * notification process. This is used to decrement the
   * notificationPendingCount and schedule another run of doNotify() when the
   * count reaches zero.
   */
  void notifyFinished();

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_SHARED_PV_SUPPORT_DEF_H
