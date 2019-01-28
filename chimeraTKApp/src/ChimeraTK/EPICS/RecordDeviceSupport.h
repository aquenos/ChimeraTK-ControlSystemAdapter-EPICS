/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2015-2019 aquenos GmbH
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

#ifndef CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_H
#define CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_H

#include <type_traits>

extern "C" {
  #include <epicsVersion.h>
}

#if EPICS_VERSION > 3 || (EPICS_VERSION == 3 && EPICS_REVISION >= 16)
#  define CHIMERATK_EPICS_LONG_STRING_SUPPORTED 1
#endif

extern "C" {
#include <aaiRecord.h>
#include <aaoRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#ifdef CHIMERATK_EPICS_LONG_STRING_SUPPORTED
#  include <lsiRecord.h>
#  include <lsoRecord.h>
#endif // CHIMERATK_EPICS_LONG_STRING_SUPPORTED
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboDirectRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
}

#include "AnalogScalarRecordDeviceSupport.h"
#include "ArrayRecordDeviceSupport.h"
#include "FixedScalarRecordDeviceSupport.h"
#include "StringScalarRecordDeviceSupport.h"
#include "detail/ToVoid.h"

namespace ChimeraTK {
namespace EPICS {

namespace detail {

/**
 * Helper template defining the various device supports.
 *
 * This template has a specialization for each supported record type and is used
 * by the RecordDeviceSupport template using declaration.
 */
template<typename RecordType>
struct RecordDeviceSupportTypeHelper;

template<>
struct RecordDeviceSupportTypeHelper<::aaiRecord> {
  using type = ArrayRecordDeviceSupport<::aaiRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::aaoRecord> {
  using type = ArrayRecordDeviceSupport<::aaoRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::aiRecord> {
  using type = AnalogScalarRecordDeviceSupport<::aiRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::aoRecord> {
  using type = AnalogScalarRecordDeviceSupport<::aoRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::biRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::biRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::boRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::boRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::longinRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::longinRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::longoutRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::longoutRecord>;
};

#ifdef CHIMERATK_EPICS_LONG_STRING_SUPPORTED
template<>
struct RecordDeviceSupportTypeHelper<::lsiRecord> {
  using type =
    StringScalarRecordDeviceSupport<::lsiRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::lsoRecord> {
  using type =
    StringScalarRecordDeviceSupport<::lsoRecord>;
};
#endif // CHIMERATK_EPICS_LONG_STRING_SUPPORTED

template<>
struct RecordDeviceSupportTypeHelper<::mbbiDirectRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbbiDirectRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbbiRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbbiRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbboDirectRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbboDirectRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbboRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbboRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::stringinRecord> {
  using type =
    StringScalarRecordDeviceSupport<::stringinRecord>;
};

template<>
struct RecordDeviceSupportTypeHelper<::stringoutRecord> {
  using type =
    StringScalarRecordDeviceSupport<::stringoutRecord>;
};

/**
 * Helper template struct for detecting whether a type has a getInterruptInfo
 * method. This is used by RecordDeviceSupportTraits.
 *
 * The first template parameter is the device-support type to be tested. The
 * second parameter is never set explicitly and defaults to void. This means
 * that the only template specializatiion is going to be used exactly if its
 * specialized parameter evaluates to void (which it does if it can be evaluated
 * at all). If the parameter cannot be evaluated, the template specialization is
 * removed by the SFINAE rule and the generic definition is used.
 */
template<typename DeviceSupportType, typename = void>
struct HasGetInterruptInfo : std::false_type {};

template<typename DeviceSupportType>
struct HasGetInterruptInfo<DeviceSupportType, ToVoid<decltype(std::declval<DeviceSupportType>().getInterruptInfo(0, std::declval<::IOSCANPVT *>()))>> : std::true_type {};

/**
 * Helper template struct for detecting whether a type has a isNoConvert method.
 * This is used by RecordDeviceSupportTraits.
 *
 * The first template parameter is the device-support type to be tested. The
 * second parameter is never set explicitly and defaults to void. This means
 * that the only template specializatiion is going to be used exactly if its
 * specialized parameter evaluates to void (which it does if it can be evaluated
 * at all). If the parameter cannot be evaluated, the template specialization is
 * removed by the SFINAE rule and the generic definition is used.
 */
template<typename DeviceSupportType, typename = void>
struct HasIsNoConvert : std::false_type {};

template<typename DeviceSupportType>
struct HasIsNoConvert<DeviceSupportType, ToVoid<decltype(std::declval<DeviceSupportType>().isNoConvert())>> : std::true_type {};

} // namespace detail


/**
 * Type of the device support class for the specified record type.
 */
template<typename RecordType>
using RecordDeviceSupport =
  typename detail::RecordDeviceSupportTypeHelper<RecordType>::type;

/**
 * Provides information about the device support for a record. The template
 * parameter is the type of the record.
 */
template<typename RecordType>
struct RecordDeviceSupportTraits {

  /**
   * Tells whether the device support is for an input or an output record.
   */
  static RecordDirection const direction = detectRecordDirection<RecordType>();

  /**
   * Tells whether the device support class has a getInterruptInfo(...) method.
   */
  static bool const hasGetInterruptInfo =
    detail::HasGetInterruptInfo<RecordDeviceSupport<RecordType>>::value;

  /**
   * Tells whether the device support class has a isNoConvert() method.
   */
  static bool const hasIsNoConvert =
    detail::HasIsNoConvert<RecordDeviceSupport<RecordType>>::value;

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_H
