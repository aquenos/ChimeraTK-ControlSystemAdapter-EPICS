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

#ifndef CHIMERATK_EPICS_CONVERTING_PV_SUPPORT_H
#define CHIMERATK_EPICS_CONVERTING_PV_SUPPORT_H

#include <typeinfo>

#include "PVSupport.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * PVSupport wrapping another PVSupport and converting between value types.
 *
 * This class makes it possible to treat a PVSupport of one type like a
 * PVSupport of a different type, by casting values. However, this conversion
 * incurs an overhead that can be significant for larger vectors. For this
 * reason, use of this class should be avoided when possible.
 */
template<typename OriginalType, typename TargetType>
class ConvertingPVSupport : public PVSupport<TargetType> {

public:

  /**
   * Type of the callback function that is called in case of an error.
   */
  using ErrorCallback = typename PVSupport<TargetType>::ErrorCallback;

  /**
   * Type of the callback passed to notify(...).
   */
  using NotifyCallback = typename PVSupport<TargetType>::NotifyCallback;

  /**
   * Type of the error callback passed to notify(...).
   */
  using NotifyErrorCallback = typename PVSupport<TargetType>::NotifyErrorCallback;

  /**
   * Type of the callback passed to read(...).
   */
  using ReadCallback = typename PVSupport<TargetType>::ReadCallback;

  /**
   * Type of a shared value vector (that is the type of value passed to a
   * NotifyCallback).
   */
  using SharedValue = typename PVSupport<TargetType>::SharedValue;

  /**
   * Type of a value vector.
   */
  using Value = typename PVSupport<TargetType>::Value;

  /**
   * Type of the callback passed to write(...).
   */
  using WriteCallback = typename PVSupport<TargetType>::WriteCallback;

  ConvertingPVSupport(
      typename PVSupport<OriginalType>::SharedPtr originalPVSupport)
      : originalPVSupport(originalPVSupport) {
  }

  // Declared in PVSupport.
  bool canNotify() override {
    return originalPVSupport->canNotify();
  }

  // Declared in PVSupport.
  bool canRead() override {
    return originalPVSupport->canRead();
  }

  // Declared in PVSupport.
  bool canWrite() override {
    return originalPVSupport->canWrite();
  }

  // Declared in PVSupport.
  std::size_t getNumberOfElements() override {
    return originalPVSupport->getNumberOfElements();
  }

  // Declared in PVSupport.
  std::tuple<Value, VersionNumber> initialValue() override {
    auto original = originalPVSupport->initialValue();
    return std::make_tuple(
      convertToTarget(std::get<0>(original)),
      std::get<1>(original));
  }

  // Declared in PVSupport.
  void notify(
      NotifyCallback const &successCallback,
      NotifyErrorCallback const &errorCallback) override {
    typename PVSupport<OriginalType>::NotifyCallback wrappedSuccessCallback;
    typename PVSupport<OriginalType>::NotifyErrorCallback wrappedErrorCallback;
    if (successCallback) {
      wrappedSuccessCallback = [successCallback](
            typename PVSupport<OriginalType>::SharedValue const &value,
            VersionNumber const &versionNumber){
          successCallback(convertToTarget(value), versionNumber);
        };
    }
    if (errorCallback) {
      wrappedErrorCallback = [errorCallback](std::exception_ptr error){
          errorCallback(error);
        };
    }
    originalPVSupport->notify(wrappedSuccessCallback, wrappedErrorCallback);
  }

  // Declared in PVSupport.
  void notifyFinished() override {
    originalPVSupport->notifyFinished();
  }

  // Declared in PVSupport.
  bool read(
      ReadCallback const &successCallback,
      ErrorCallback const &errorCallback) override {
    typename PVSupport<OriginalType>::ReadCallback wrappedSuccessCallback;
    typename PVSupport<OriginalType>::ErrorCallback wrappedErrorCallback;
    if (successCallback) {
      wrappedSuccessCallback = [successCallback](
            bool immediate,
            typename PVSupport<OriginalType>::SharedValue const &value,
            VersionNumber const &versionNumber){
          successCallback(
            immediate, convertToTarget(value), versionNumber);
        };
    }
    if (errorCallback) {
      wrappedErrorCallback = [errorCallback](
            bool immediate, std::exception_ptr error){
          errorCallback(immediate, error);
        };
    }
    return originalPVSupport->read(wrappedSuccessCallback, wrappedErrorCallback);
  }

  // Declared in PVSupport.
  void willWrite() override {
    originalPVSupport->willWrite();
  }

  // Declared in PVSupport.
  bool write(Value const &value,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) override {
    typename PVSupport<OriginalType>::WriteCallback wrappedSuccessCallback;
    typename PVSupport<OriginalType>::ErrorCallback wrappedErrorCallback;
    if (successCallback) {
      wrappedSuccessCallback = [successCallback](bool immediate){
          successCallback(immediate);
        };
    }
    if (errorCallback) {
      wrappedErrorCallback = [errorCallback](
            bool immediate, std::exception_ptr error){
          errorCallback(immediate, error);
        };
    }
    return originalPVSupport->write(
      convertToOriginal(value), wrappedSuccessCallback, wrappedErrorCallback);
  }

  // Declared in PVSupport.
  bool write(Value &&value,
      WriteCallback const &successCallback,
      ErrorCallback const &errorCallback) override {
    // When we convert, we have to copy anyway, so we can simply treat the
    // rvalue reference as a const lvalue reference.
    return this->write(
      static_cast<Value const &>(value), successCallback, errorCallback);
  }

private:

  typename PVSupport<OriginalType>::SharedPtr originalPVSupport;

  static typename PVSupport<OriginalType>::Value convertToOriginal(
      Value const &targetValue) {
    typename PVSupport<OriginalType>::Value originalValue;
    originalValue.reserve(targetValue.size());
    for (TargetType v : targetValue) {
      originalValue.push_back(static_cast<OriginalType>(v));
    }
    return originalValue;
  }

  static Value convertToTarget(
      typename PVSupport<OriginalType>::Value const &originalValue) {
    // TODO Make sure that this class is not used when the original and target
    // types are the same. Adding this optimization in the calling code is much
    // simpler than adding it here.
    Value targetValue;
    targetValue.reserve(originalValue.size());
    for (OriginalType v : originalValue) {
      targetValue.push_back(static_cast<TargetType>(v));
    }
    return targetValue;
  }

  static SharedValue convertToTarget(
      typename PVSupport<OriginalType>::SharedValue const &originalValue) {
    // TODO Make sure that this class is not used when the original and target
    // types are the same. Adding this optimization in the calling code is much
    // simpler than adding it here.
    auto targetValue = std::make_shared<Value>();
    targetValue->reserve(originalValue->size());
    for (OriginalType v : *originalValue) {
      targetValue->push_back(static_cast<TargetType>(v));
    }
    return targetValue;
  }

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONVERTING_PV_SUPPORT_H
