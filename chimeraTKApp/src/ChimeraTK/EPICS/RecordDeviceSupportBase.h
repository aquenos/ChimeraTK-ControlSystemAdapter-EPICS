/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2015-2022 aquenos GmbH
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

#ifndef CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_BASE_H
#define CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_BASE_H

#include <memory>
#include <stdexcept>
#include <typeinfo>

#include "PVProviderRegistry.h"
#include "RecordAddress.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * Base functionality shared by all record device support classes.
 */
class RecordDeviceSupportBase {

public:

  /**
   * Constructor. Takes a parsed contents of the record's link field (typically
   * INP or OUT) that is used to determine the application or device name, the
   * process-variable name, and the value type.
   */
  RecordDeviceSupportBase(RecordAddress const & address)
      : noBidirectional(address.isNoBidirectional()),
        pvName(address.getProcessVariableName()),
        pvProvider(PVProviderRegistry::getPVProvider(
          address.getApplicationOrDeviceName())),
        pvSupport(
          callForValueTypeInternal<CallCreatePVSupport>(
            (address.hasValueType() ? address.getValueType()
              : pvProvider->getDefaultType(pvName)),
            this)),
        valueType(address.hasValueType() ? address.getValueType()
          : pvProvider->getDefaultType(pvName)) {
  }

protected:

  /**
   * Flag indicating whether support for bidirectional process variables shall
   * be disabled for this record. This option is only relevant for output
   * records.
   */
  bool const noBidirectional;

  /**
   * Name of the process variable.
   */
  std::string const pvName;

  /**
   * Pointer to the process variable support.
   */
  PVProvider::SharedPtr const pvProvider;

  /**
   * Shared pointer to the original PV support.
   */
  std::shared_ptr<PVSupportBase> const pvSupport;

  /**
   * Type of the process variable's values. This can be the default type or a
   * type explicitly specified by the user.
   */
  std::type_info const &valueType;

  /**
   * Instantiates and calls a function object template for the value type of the
   * PV support of this instance.
   *
   * In order for this to work correctly, supplied template class must be
   * default constructible, it must define operator(). The template must be
   * instantiable for all possible value types and the return value of
   * operator() must not depend on the value type.
   *
   * This version of the template function is intended for function objects
   * where operator() does not take any arguments.
   */
  template<template<typename> class F, typename R=decltype(F<std::int8_t>()())>
  R callForValueType() {
    if (this->valueType == typeid(std::int8_t)) {
      return F<std::int8_t>()();
    } else if (this->valueType == typeid(std::uint8_t)) {
      return F<std::uint8_t>()();
    } else if (this->valueType == typeid(std::int16_t)) {
      return F<std::int16_t>()();
    } else if (this->valueType == typeid(std::uint16_t)) {
      return F<std::uint16_t>()();
    } else if (this->valueType == typeid(std::int32_t)) {
      return F<std::int32_t>()();
    } else if (this->valueType == typeid(std::uint32_t)) {
      return F<std::uint32_t>()();
    } else if (this->valueType == typeid(std::int64_t)) {
      return F<std::int64_t>()();
    } else if (this->valueType == typeid(std::uint64_t)) {
      return F<std::uint64_t>()();
    } else if (this->valueType == typeid(float)) {
      return F<float>()();
    } else if (this->valueType == typeid(double)) {
      return F<double>()();
    } else if (this->valueType == typeid(std::string)) {
      return F<std::string>()();
    } else if (this->valueType == typeid(ChimeraTK::Boolean)) {
      return F<ChimeraTK::Boolean>()();
    } else if (this->valueType == typeid(ChimeraTK::Void)) {
      return F<ChimeraTK::Void>()();
    } else {
      throw std::logic_error(
        std::string("Unexpected value type: ") + valueType.name());
    }
  }

  /**
   * Instantiates and calls a function object template for the value type of the
   * PV support of this instance. This is very similar to callForValueType(),
   * but does not include support for ChimeraTK::Void.
   *
   * In order for this to work correctly, supplied template class must be
   * default constructible, it must define operator(). The template must be
   * instantiable for all possible value types and the return value of
   * operator() must not depend on the value type.
   *
   * This version of the template function is intended for function objects
   * where operator() does not take any arguments.
   */
  template<template<typename> class F, typename R=decltype(F<std::int8_t>()())>
  R callForValueTypeNoVoid() {
    if (this->valueType == typeid(std::int8_t)) {
      return F<std::int8_t>()();
    } else if (this->valueType == typeid(std::uint8_t)) {
      return F<std::uint8_t>()();
    } else if (this->valueType == typeid(std::int16_t)) {
      return F<std::int16_t>()();
    } else if (this->valueType == typeid(std::uint16_t)) {
      return F<std::uint16_t>()();
    } else if (this->valueType == typeid(std::int32_t)) {
      return F<std::int32_t>()();
    } else if (this->valueType == typeid(std::uint32_t)) {
      return F<std::uint32_t>()();
    } else if (this->valueType == typeid(std::int64_t)) {
      return F<std::int64_t>()();
    } else if (this->valueType == typeid(std::uint64_t)) {
      return F<std::uint64_t>()();
    } else if (this->valueType == typeid(float)) {
      return F<float>()();
    } else if (this->valueType == typeid(double)) {
      return F<double>()();
    } else if (this->valueType == typeid(std::string)) {
      return F<std::string>()();
    } else if (this->valueType == typeid(ChimeraTK::Boolean)) {
      return F<ChimeraTK::Boolean>()();
    } else if (this->valueType == typeid(ChimeraTK::Void)) {
      throw std::logic_error(
        std::string("Unsupported value type: void"));
    } else {
      throw std::logic_error(
        std::string("Unexpected value type: ") + valueType.name());
    }
  }

  /**
   * Instantiates and calls a function object template for the value type of the
   * PV support of this instance.
   *
   * In order for this to work correctly, supplied template class must be
   * default constructible, it must define operator(). The template must be
   * instantiable for all possible value types and the return value of
   * operator() must not depend on the value type.
   *
   * This version of the template function is intended for function objects
   * where operator() does take at least one argument.
   */
  template<template<typename> class F, typename... Args, typename R=decltype(F<std::int8_t>()(std::declval<Args>()...))>
  R callForValueType(Args... args) {
    return RecordDeviceSupportBase::callForValueTypeInternal<F, Args...>(
      this->valueType, std::forward<Args>(args)...);
  }

  /**
   * Instantiates and calls a function object template for the value type of the
   * PV support of this instance. This is similar to callForValueType, but does
   * not support ChimeraTK::Void.
   *
   * In order for this to work correctly, supplied template class must be
   * default constructible, it must define operator(). The template must be
   * instantiable for all possible value types and the return value of
   * operator() must not depend on the value type.
   *
   * This version of the template function is intended for function objects
   * where operator() does take at least one argument.
   */
  template<template<typename> class F, typename... Args, typename R=decltype(F<std::int8_t>()(std::declval<Args>()...))>
  R callForValueTypeNoVoid(Args... args) {
    return RecordDeviceSupportBase::callForValueTypeNoVoidInternal<F, Args...>(
      this->valueType, std::forward<Args>(args)...);
  }

  /**
   * Returns a pointer to the PV support for this record. The PV support's
   * element type has to be specified as a template parameter. Throws an
   * exception if the PV support's type does not match the specified type.
   */
  template<typename T>
  inline typename PVSupport<T>::SharedPtr getPVSupport() const {
    if (typeid(T) != this->valueType) {
      throw std::logic_error(
        std::string("PV support is of type ") + this->valueType.name()
        + "', but type '" + typeid(T).name() + "' has been requested.");
    }
    return std::dynamic_pointer_cast<PVSupport<T>>(this->pvSupport);
  }

private:

  /**
   * Helper template for calling the right instantiation of
   * createProcessVariableSupport for the current value type.
   */
  template<typename T>
  struct CallCreatePVSupport {
    PVSupportBase::SharedPtr operator()(RecordDeviceSupportBase *obj) {
      return obj->pvProvider->template createPVSupport<T>(
        obj->pvName);
    }
  };

  /**
   * Instantiates and calls a function object template for the specified value
   * type.
   *
   * In order for this to work correctly, supplied template class must be
   * default constructible, it must define operator(). The template must be
   * instantiable for all possible value types and the return value of
   * operator() must not depend on the value type.
   *
   * This template function is used by the protected callForValueType template
   * function and by the constructor (which needs it before the valueTypeField
   * has been initialized).
   */
  template<template<typename> class F, typename... Args, typename R=decltype(F<std::int8_t>()(std::declval<Args>()...))>
  static R callForValueTypeInternal(std::type_info const &valueType, Args... args) {
    if (valueType == typeid(std::int8_t)) {
      return F<std::int8_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint8_t)) {
      return F<std::uint8_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::int16_t)) {
      return F<std::int16_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint16_t)) {
      return F<std::uint16_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::int32_t)) {
      return F<std::int32_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint32_t)) {
      return F<std::uint32_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::int64_t)) {
      return F<std::int64_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint64_t)) {
      return F<std::uint64_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(float)) {
      return F<float>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(double)) {
      return F<double>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::string)) {
      return F<std::string>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(ChimeraTK::Boolean)) {
      return F<ChimeraTK::Boolean>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(ChimeraTK::Void)) {
      return F<ChimeraTK::Void>()(std::forward<Args>(args)...);
    } else {
      throw std::logic_error(
        std::string("Unexpected value type: ") + valueType.name());
    }
  }

  /**
   * Instantiates and calls a function object template for the specified value
   * type. This is similar to callForValueTypeInternal, but does not support
   * ChimeraTK::Void.
   *
   * In order for this to work correctly, supplied template class must be
   * default constructible, it must define operator(). The template must be
   * instantiable for all possible value types and the return value of
   * operator() must not depend on the value type.
   *
   * This template function is used by the protected callForValueType template
   * function and by the constructor (which needs it before the valueTypeField
   * has been initialized).
   */
  template<template<typename> class F, typename... Args, typename R=decltype(F<std::int8_t>()(std::declval<Args>()...))>
  static R callForValueTypeNoVoidInternal(std::type_info const &valueType, Args... args) {
    if (valueType == typeid(std::int8_t)) {
      return F<std::int8_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint8_t)) {
      return F<std::uint8_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::int16_t)) {
      return F<std::int16_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint16_t)) {
      return F<std::uint16_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::int32_t)) {
      return F<std::int32_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint32_t)) {
      return F<std::uint32_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::int64_t)) {
      return F<std::int64_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::uint64_t)) {
      return F<std::uint64_t>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(float)) {
      return F<float>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(double)) {
      return F<double>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(std::string)) {
      return F<std::string>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(ChimeraTK::Boolean)) {
      return F<ChimeraTK::Boolean>()(std::forward<Args>(args)...);
    } else if (valueType == typeid(ChimeraTK::Void)) {
      throw std::logic_error(
        std::string("Unsupported value type: void"));
    } else {
      throw std::logic_error(
        std::string("Unexpected value type: ") + valueType.name());
    }
  }

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_BASE_H
