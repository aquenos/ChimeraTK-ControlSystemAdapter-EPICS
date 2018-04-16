/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2015-2018 aquenos GmbH
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
      : pvName(address.getProcessVariableName()),
        pvProvider(PVProviderRegistry::getPVProvider(
          address.getApplicationOrDeviceName())),
        valueType(address.hasValueType() ? address.getValueType()
          : pvProvider->getDefaultType(pvName)) {
    this->pvSupport = this->callForValueType<CallCreatePVSupport>(this);
  }

protected:

  /**
   * Name of the process variable.
   */
  std::string pvName;

  /**
   * Pointer to the process variable support.
   */
  PVProvider::SharedPtr pvProvider;

  /**
   * Shared pointer to the original PV support.
   */
  std::shared_ptr<PVSupportBase> pvSupport;

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
    } else if (this->valueType == typeid(float)) {
      return F<float>()();
    } else if (this->valueType == typeid(double)) {
      return F<double>()();
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
    if (this->valueType == typeid(std::int8_t)) {
      return F<std::int8_t>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(std::uint8_t)) {
      return F<std::uint8_t>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(std::int16_t)) {
      return F<std::int16_t>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(std::uint16_t)) {
      return F<std::uint16_t>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(std::int32_t)) {
      return F<std::int32_t>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(std::uint32_t)) {
      return F<std::uint32_t>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(float)) {
      return F<float>()(std::forward<Args>(args)...);
    } else if (this->valueType == typeid(double)) {
      return F<double>()(std::forward<Args>(args)...);
    } else {
      throw std::logic_error(
        std::string("Unexpected value type: ") + valueType.name());
    }
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

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_BASE_H
