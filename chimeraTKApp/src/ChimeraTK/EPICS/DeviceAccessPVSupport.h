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

#ifndef CHIMERATK_EPICS_DEVICE_ACCESS_PV_SUPPORT_H
#define CHIMERATK_EPICS_DEVICE_ACCESS_PV_SUPPORT_H

#include <exception>
#include <memory>
#include <utility>

#include "DeviceAccessPVProviderDef.h"
#include "PVSupport.h"
#include "errorPrint.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * PVSupport implementation used by the DeviceAccessPVProvider.
 *
 * This class uses an accessor for accessing a register in a ChimeraTK
 * Device Access device.
 */
template<typename T>
class DeviceAccessPVSupport :
    public PVSupport<T>,
    public std::enable_shared_from_this<DeviceAccessPVSupport<T>> {

public:

  /**
   * Type of the callback function that is called in case of an error.
   */
  using ErrorCallback = typename PVSupport<T>::ErrorCallback;

  /**
   * Type of the callback passed to read(...).
   */
  using ReadCallback = typename PVSupport<T>::ReadCallback;

  /**
   * Type of a shared value vector (that is the type of value passed to a
   * NotifyCallback or ReadCallback).
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
   * Creates a new PV support for the specified PV provider and register name.
   */
  DeviceAccessPVSupport(
      DeviceAccessPVProvider::SharedPtr provider,
      std::string const &registerName);

  virtual ~DeviceAccessPVSupport() noexcept;

  // Declared in PVSupport.
  virtual bool canRead() override;

  // Declared in PVSupport.
  virtual bool canWrite() override;

  // Declared in PVSupport.
  virtual std::size_t getNumberOfElements() override;

  // Declared in PVSupport.
  virtual std::tuple<Value, VersionNumber> initialValue() override;

  // Declared in PVSupport.
  virtual bool read(
      ReadCallback const &successCallback,
      ErrorCallback const &errorCallback) override;

  // Declared in PVSupport.
  virtual bool write(Value const &value,
      VersionNumber const &versionNumber,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) override;

  // Declared in PVSupport.
  virtual bool write(Value &&value,
      VersionNumber const &versionNumber,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) override;

private:

  /**
   * Accessor that is used for accessing the process variable.
   */
  OneDRegisterAccessor<T> accessor;

  /**
   * PV provider that created this instance.
   */
  DeviceAccessPVProvider::SharedPtr provider;

};

template<typename T>
DeviceAccessPVSupport<T>::DeviceAccessPVSupport(
    DeviceAccessPVProvider::SharedPtr provider,
    std::string const &registerName)
    : accessor(provider->device.template getOneDRegisterAccessor<T>(registerName)),
      provider(provider) {
}

template<typename T>
DeviceAccessPVSupport<T>::~DeviceAccessPVSupport() noexcept {
}

template<typename T>
bool DeviceAccessPVSupport<T>::canRead() {
  return this->accessor.isReadable();
}

template<typename T>
bool DeviceAccessPVSupport<T>::canWrite() {
  return this->accessor.isWriteable();
}

template<typename T>
std::size_t DeviceAccessPVSupport<T>::getNumberOfElements() {
  return this->accessor.getNElements();
}

template<typename T>
std::tuple<typename PVSupport<T>::Value, VersionNumber> DeviceAccessPVSupport<T>::initialValue() {
  this->accessor.read();
  Value value(this->accessor.getNElements());
  this->accessor.swap(value);
  return std::make_tuple(
      std::move(value),
      VersionNumber());
}

template<typename T>
bool DeviceAccessPVSupport<T>::read(
    ReadCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  // We pass a shared pointer to this instead of the raw this pointer into the
  // lambda expression. If this instance got destroyed before or while the
  // lambda expression was running, the raw pointer would be invalid. This
  // cannot happen when using a shared pointer.
  auto sharedThis = this->shared_from_this();
  bool immediate = this->provider->isSynchronous();
  this->provider->submitIoTask(
    [sharedThis, immediate, successCallback, errorCallback](){
      Value value(sharedThis->accessor.getNElements());
      try {
        sharedThis->accessor.read();
        sharedThis->accessor.swap(value);
      } catch (...) {
        try {
          errorCallback(immediate, std::current_exception());
          return;
        } catch (std::exception &e) {
          errorPrintf(
            "A read callback threw an exception. This indicates a bug in the record device support code. The exception message was: %s",
            e.what());
        } catch (...) {
          errorPrintf(
            "A read callback threw an exception. This indicates a bug in the record device support code.");
        }
      }
      try {
        successCallback(immediate,
          std::make_shared<Value const>(std::move(value)),
          VersionNumber());
      } catch (std::exception &e) {
        errorPrintf(
          "A read callback threw an exception. This indicates a bug in the record device support code. The exception message was: %s",
          e.what());
      } catch (...) {
        errorPrintf(
          "A read callback threw an exception. This indicates a bug in the record device support code.");
      }
  });
  return immediate;
}

template<typename T>
bool DeviceAccessPVSupport<T>::write(
    Value const &value,
    VersionNumber const &versionNumber,
    WriteCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  // The write method that takes an rvalue swaps values, so creating a copy of
  // the original vector here and then passing this copy is only slightly less
  // efficient than actually copying to the destination vector, but it
  // simplifies the code significantly.
  Value valueCopy(value);
  return this->write(
      std::move(valueCopy), versionNumber, successCallback, errorCallback);
}

template<typename T>
bool DeviceAccessPVSupport<T>::write(
    Value &&value,
    VersionNumber const &versionNumber,
    WriteCallback const &successCallback,
    ErrorCallback const &errorCallback) {
  // We pass a shared pointer to this instead of the raw this pointer into the
  // lambda expression. If this instance got destroyed before or while the
  // lambda expression was running, the raw pointer would be invalid. This
  // cannot happen when using a shared pointer.
  auto sharedThis = this->shared_from_this();
  this->accessor.swap(value);
  bool immediate = this->provider->isSynchronous();
  this->provider->submitIoTask(
    [sharedThis, immediate, successCallback, errorCallback, versionNumber](){
      try {
        sharedThis->accessor.write(versionNumber);
      } catch (...) {
        try {
          errorCallback(immediate, std::current_exception());
          return;
        } catch (std::exception &e) {
          errorPrintf(
            "A write callback threw an exception. This indicates a bug in the record device support code. The exception message was: %s",
            e.what());
        } catch (...) {
          errorPrintf(
            "A write callback threw an exception. This indicates a bug in the record device support code.");
        }
      }
      try {
        successCallback(immediate);
      } catch (std::exception &e) {
        errorPrintf(
          "A write callback threw an exception. This indicates a bug in the record device support code. The exception message was: %s",
          e.what());
      } catch (...) {
        errorPrintf(
          "A write callback threw an exception. This indicates a bug in the record device support code.");
      }
  });
  return immediate;
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_DEVICE_ACCESS_PV_SUPPORT_H
