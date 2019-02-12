
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

#ifndef CHIMERATK_EPICS_PV_SUPPORT_H
#define CHIMERATK_EPICS_PV_SUPPORT_H

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

#include <ChimeraTK/VersionNumber.h>

namespace ChimeraTK {
namespace EPICS {

/**
 * Base interface for all PVSupport instances. This base interface defines some
 * functions for inspecting the PVSupport, but it cannot be used for any actions
 * as these actions will typically depend on the value type.
 */
class PVSupportBase {

public:

  /**
   * Type of a shared pointer to this type.
   */
  using SharedPtr = std::shared_ptr<PVSupportBase>;

  /**
   * Tells whether this PV support supports notifications. If this method
   * returns false, calling notify(...) will result in an exception.
   */
  virtual bool canNotify() {
    return false;
  }

  /**
   * Tells whether this PV support allows reading values. If this method returns
   * false, calling read(...) will result in an exception.
   */
  virtual bool canRead() {
    return false;
  }

  /**
   * Tells whether this PV support allows writing values. If this method returns
   * false, calling write(...) will result in an exception.
   */
  virtual bool canWrite() {
    return false;
  }

  /**
   * Returns the number of elements each value of the process variable has.
   */
  virtual std::size_t getNumberOfElements() = 0;

protected:

  /**
   * Destructor. The destructor is protected so that instances cannot be
   * destroyed through a pointer to this interface.
   */
  virtual ~PVSupportBase() noexcept {
  };

};

/**
 * Interface for types that provide a binding between the EPICS record and the
 * backend code.
 *
 * There are two implementations of this interface: The
 * ControlSystemAdapterPVSupport (which is backed by a ProcessArray from the
 * ChimeraTK Control System Adapter) and the DeviceAccessPVSupport (which is
 * backed by a OneDRegisterAccessor from ChimeraTK Device Access).
 *
 * Each PVSupport instance is not thread safe, but different PVSupport instances
 * are, even if they internally refer to the same process variable. This means
 * that code that wants to access a PVSupport from multiple threads should
 * instead create a separate instance for each thread.
 *
 * Special care must be taken when calling the read(...) and write(...) methods:
 * When either of these methods signals that it will finish asynchronously,
 * neither of them must be called until the previous call has finished
 * asynchronously. Calling either of these method any earlier results in
 * undefined behavior.
 */
template<typename T>
class PVSupport : public PVSupportBase {

public:

  /**
   * Type of the elements of the value vector.
   */
  using ElementType = T;

  /**
   * Type of the callback function that is called when there is an error. The
   * first argument to the callback function is a flag indicating whether the
   * callback function is called in-thread, before the method that got this
   * callback passed returns (true) or it is called at a later point in time,
   * after that method has returned (false).
   */
  using ErrorCallback = std::function<void(bool, std::exception_ptr const &)>;

  /**
   * Type of the value vector.
   */
  using Value = std::vector<ElementType>;

  /**
   * Type of a shared pointer to this type.
   */
  using SharedPtr = std::shared_ptr<PVSupport>;

  /**
   * Type of a pointer to a shared value vector. A shared value vector is passed
   * when calling callbacks. The shared value is valid as long as any code holds
   * a reference to it and cannot be modified.
   */
  using SharedValue = std::shared_ptr<Value const>;

  /**
   * Type of the callback function that is called when a new value is available.
   * Such a function can be registered by calling notify(...);
   */
  using NotifyCallback = std::function<void(SharedValue const &, VersionNumber const &)>;

  /**
   * Type of the callback function that is called when there is an error for a
   * notification subscription.
   */
  using NotifyErrorCallback = std::function<void(std::exception_ptr const &)>;

  /**
   * Type of the callback function that is called when a value is successfully
   * read by calling read(...). The first argument to the callback function is a
   * flag indicating whether the callback function is called in-thread, before
   * read(...) returns (true) or it is called at a later point in time, after
   * read(...) has returned (false).
   */
  using ReadCallback = std::function<void(bool, SharedValue const &, VersionNumber const &)>;

  /**
   * Type of the callback function that is called when a value is successfully
   * written by calling write(...). The argument to the callback function is a
   * flag indicating whether the callback function is called in-thread, before
   * write(...) returns (true) or it is called at a later point in time, after
   * write(...) has returned (false).
   */
  using WriteCallback = std::function<void(bool)>;

  /**
   * Cancels a request for notifications. Due to how notifications are
   * implemented, one more notification might be received if there is a
   * notification in progress while calling this method.
   *
   * This method throws an exception if canNotify() returns false.
   */
  virtual void cancelNotify() {
    notify(nullptr, nullptr);
  }

  /**
   * Returns the initial value of the process variable. This method is pretty
   * much tailored to the needs of initializing an output record in EPICS.
   *
   * This method is similar to the read(...) method, but it differs in several
   * aspects: First, this method works synchronously. This means that it will
   * block until it can return a value. Second, this method might be used on
   * PVs that are not readable. However, whether it can be used for such PVs,
   * depends on the implementation. Third, this method might not necessarily
   * try to transfer a new value from the backing implementation and might
   * simply use the latest known values instead.
   *
   * In summary, this method should only be used to initialize output records.
   *
   * This method throws an exception when the implementation cannot provide an
   * initial value for the respective process variable.
   */
  virtual std::tuple<Value, VersionNumber> initialValue();

  /**
   * Requests notifications. The successCallback is called every time there is
   * a new value available. The errorCallback is called when there is an error.
   *
   * The successCallback must call notifyFinished() (right when it is called or
   * indirectly at a later point in time). Failure to do so will result in no
   * more notifications being delivered.
   *
   * This method throws an exception if canNotify() returns false.
   */
  virtual void notify(
      NotifyCallback const &successCallback,
      NotifyErrorCallback const &errorCallback) {
    throw std::logic_error(
      "This PV support does not support notifications. Check by calling canNotify() before calling this method.");
  }

  /**
   * Indicates that a notification has been handled. The callback passed to
   * notify(...) must call this method (right when it is called or indirectly at
   * a later point in time).Failure to do so will result in no more
   * notifications being delivered.
   *
   * This method throws an exception if canNotify() returns false.
   */
  virtual void notifyFinished() {
    throw std::logic_error(
      "This PV support does not support notifications. Check by calling canNotify() before calling this method.");
  }

  /**
   * Reads the process variable's value. When the value is read successfully,
   * the successCallback is called. If the read attempt fails, the errorCallback
   * is called.
   *
   * The value passed to the successCallback might not necessarily be the newest
   * value for the PV. It might simply be the next value from a queue of
   * available value or even the value from the last call of this method, when
   * notifications are in progress. These details are defined by the
   * implementing class.
   *
   * If the callback has been called in-thread, this method returns true. If the
   * callback is going to be notified asynchronously, this method returns false.
   *
   * This method throws an exception if canRead() returns false.
   *
   * When this method returns false, special care must be taken: As this means
   * that this method will will finish asynchronously, neither read nor write
   * must be called until the previous call has finished asynchronously. Calling
   * either of these method any earlier results in undefined behavior.
   */
  virtual bool read(
      ReadCallback const &successCallback,
      ErrorCallback const &errorCallback) {
    throw std::logic_error(
      "This PV support does not allow reading. Check by calling canRead() before calling this method.");
  }

  /**
   * Writes a value to the process variable. When the value is written
   * successfully, the successCallback is called. If the write attempt
   * fails, the errorCallback is called.
   *
   * If the callback has been called in-thread, this method returns true. If the
   * callback is going to be notified asynchronously, this method returns false.
   *
   * This method throws an exception if canWrite() returns false.
   *
   * When this method returns false, special care must be taken: As this means
   * that this method will will finish asynchronously, neither read nor write
   * must be called until the previous call has finished asynchronously. Calling
   * either of these method any earlier results in undefined behavior.
   */
  virtual bool write(
      Value const &value,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) {
    throw std::logic_error(
      "This PV support does not allow writing. Check by calling canWrite() before calling this method.");
  }

  /**
   * Writes a value to the process variable. When the value is written
   * successfully, the successCallback is called. If the write attempt
   * fails, the errorCallback is called.
   *
   * If the callback has been called in-thread, this method returns true. If the
   * callback is going to be notified asynchronously, this method returns false.
   *
   * This method throws an exception if canWrite() returns false.
   *
   * When this method returns false, special care must be taken: As this means
   * that this method will will finish asynchronously, neither read nor write
   * must be called until the previous call has finished asynchronously. Calling
   * either of these method any earlier results in undefined behavior.
   */
  virtual bool write(
      Value &&value,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) {
    throw std::logic_error(
      "This PV support does not allow writing. Check by calling canWrite() before calling this method.");
  }

protected:

  /**
   * Destructor. The destructor is protected so that instances cannot be
   * destroyed through a pointer to this interface.
   */
  virtual ~PVSupport() noexcept {
  }

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_PV_SUPPORT_H
