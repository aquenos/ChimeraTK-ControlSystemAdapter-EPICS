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

#ifndef CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_H
#define CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_H

extern "C" {
#include <aaiRecord.h>
#include <aaoRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboDirectRecord.h>
}

#include "AnalogScalarRecordDeviceSupport.h"
#include "ArrayRecordDeviceSupport.h"
#include "FixedScalarRecordDeviceSupport.h"

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
  using type = ArrayRecordDeviceSupport<::aaiRecord, RecordDirection::INPUT>;
};

template<>
struct RecordDeviceSupportTypeHelper<::aaoRecord> {
  using type = ArrayRecordDeviceSupport<::aaoRecord, RecordDirection::OUTPUT>;
};

template<>
struct RecordDeviceSupportTypeHelper<::aiRecord> {
  using type = AnalogScalarRecordDeviceSupport<::aiRecord, RecordDirection::INPUT>;
};

template<>
struct RecordDeviceSupportTypeHelper<::aoRecord> {
  using type = AnalogScalarRecordDeviceSupport<::aoRecord, RecordDirection::OUTPUT>;
};

template<>
struct RecordDeviceSupportTypeHelper<::biRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::biRecord, RecordDirection::INPUT, RecordValueFieldName::RVAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::boRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::boRecord, RecordDirection::OUTPUT, RecordValueFieldName::RVAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::longinRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::longinRecord, RecordDirection::INPUT, RecordValueFieldName::VAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::longoutRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::longoutRecord, RecordDirection::OUTPUT, RecordValueFieldName::VAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbbiDirectRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbbiDirectRecord, RecordDirection::INPUT, RecordValueFieldName::RVAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbbiRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbbiRecord, RecordDirection::INPUT, RecordValueFieldName::RVAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbboDirectRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbboDirectRecord, RecordDirection::OUTPUT, RecordValueFieldName::RVAL>;
};

template<>
struct RecordDeviceSupportTypeHelper<::mbboRecord> {
  using type =
    FixedScalarRecordDeviceSupport<::mbboRecord, RecordDirection::OUTPUT, RecordValueFieldName::RVAL>;
};

} // namespace detail

/**
 * Type of the device support class for the specified record type.
 */
template<typename RecordType>
using RecordDeviceSupport =
  typename detail::RecordDeviceSupportTypeHelper<RecordType>::type;

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_DEVICE_SUPPORT_H
