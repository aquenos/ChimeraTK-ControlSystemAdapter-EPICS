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

#ifndef CHIMERATK_EPICS_ANALOG_SCALAR_RECORD_DEVICE_SUPPORT_H
#define CHIMERATK_EPICS_ANALOG_SCALAR_RECORD_DEVICE_SUPPORT_H

#include <memory>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

extern "C" {
#include <callback.h>
#include <dbLink.h>
#include <dbScan.h>
} // extern "C"

#include "FixedScalarRecordDeviceSupport.h"
#include "RecordDirection.h"

namespace ChimeraTK {
namespace EPICS {

namespace detail {

/**
 * Base class for AnalogScalarRecordDeviceSupport. This class implements the
 * code that is shared by the device supports for input and output records.
 *
 * The template parameter are the record's data-structure type and its direction
 * (input or output).
 */
template<typename RecordType, RecordDirection Direction>
class AnalogScalarRecordDeviceSupportTrait {

public:

  /**
   * Constructor. Takes a pointer to the record's data-structure and a reference
   * to the INP or OUT field.
   */
  AnalogScalarRecordDeviceSupportTrait(
      RecordType *record, ::DBLINK const &linkField) {
    auto address = RecordAddress::parse(linkField);
    auto pvProvider = PVProviderRegistry::getPVProvider(
      address.getApplicationOrDeviceName());
    std::type_info const &valueType(
      address.hasValueType() ? address.getValueType()
      : pvProvider->getDefaultType(address.getProcessVariableName()));
    this->noConvert = valueType == typeid(float) || valueType == typeid(double);
    if (this->isNoConvert()) {
      this->valDeviceSupport =
        std::make_unique<FixedScalarRecordDeviceSupport<RecordType, Direction, RecordValueFieldName::VAL>>(
          record);
    } else {
      this->rvalDeviceSupport =
        std::make_unique<FixedScalarRecordDeviceSupport<RecordType, Direction, RecordValueFieldName::RVAL>>(
          record);
    }
  }

  /**
   * Tells whether the device support should access the VAL field directly,
   * instead of using the RVAL field. Returns true if the process variable is
   * of a floating-point type and thus the VAL field should be used directly.
   */
  inline bool isNoConvert() {
    return this->noConvert;
  }

  /**
   * Starts a read operation, completes a read operation (depending on the
   * current state). When the record is in I/O Intr mode, this method is also
   * called to process a value that has been made available by the notify
   * callback.
   */
  inline void process() {
    if (this->isNoConvert()) {
      this->valDeviceSupport->process();
    } else {
      this->rvalDeviceSupport->process();
    }
  }

protected:

  /**
   * Flag indicating whether the device support should use the VAL field (true)
   * or the RVAL field (false).
   */
  bool noConvert;

  /**
   * Pointer the underlying device support. This pointer is only initialized
   * noConvert is false.
   */
  std::unique_ptr<FixedScalarRecordDeviceSupport<RecordType, Direction, RecordValueFieldName::RVAL>> rvalDeviceSupport;

  /**
   * Pointer the underlying device support. This pointer is only initialized
   * noConvert is true.
   */
  std::unique_ptr<FixedScalarRecordDeviceSupport<RecordType, Direction, RecordValueFieldName::VAL>> valDeviceSupport;

};

} // namespace detail

/**
 * Record device support class for the ai and ao records.
 *
 * The template parameters are the record's data-structure type and the direction
 * of the record (input or output).
 */
template<typename RecordType, RecordDirection Direction = detectRecordDirection<RecordType>()>
class AnalogScalarRecordDeviceSupport;

// Template specialization for input records.
template<typename RecordType>
class AnalogScalarRecordDeviceSupport<RecordType, RecordDirection::INPUT>
    : public detail::AnalogScalarRecordDeviceSupportTrait<RecordType, RecordDirection::INPUT> {

public:

  /**
   * Constructor. Takes a pointer to the record's data-structure.
   */
  AnalogScalarRecordDeviceSupport(RecordType *record)
      : detail::AnalogScalarRecordDeviceSupportTrait<RecordType, RecordDirection::INPUT>(
          record, record->inp) {
  }

  /**
   * Processes a request to enable or disable the I/O Intr mode.
   */
  inline void getInterruptInfo(int command, ::IOSCANPVT *iopvt) {
    if (this->isNoConvert()) {
      this->valDeviceSupport->getInterruptInfo(command, iopvt);
    } else {
      this->rvalDeviceSupport->getInterruptInfo(command, iopvt);
    }
  }

};

// Template specialization for output records.
template<typename RecordType>
class AnalogScalarRecordDeviceSupport<RecordType, RecordDirection::OUTPUT>
    : public detail::AnalogScalarRecordDeviceSupportTrait<RecordType, RecordDirection::OUTPUT> {

public:

  /**
   * Constructor. Takes a pointer to the record's data-structure.
   */
  AnalogScalarRecordDeviceSupport(RecordType *record)
      : detail::AnalogScalarRecordDeviceSupportTrait<RecordType, RecordDirection::OUTPUT>(
          record, record->out) {
  }

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_ANALOG_SCALAR_RECORD_DEVICE_SUPPORT_H
