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

#ifndef CHIMERATK_EPICS_RECORD_DIRECTION_H
#define CHIMERATK_EPICS_RECORD_DIRECTION_H

#include "detail/ToVoid.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * Data direction of a record.
 */
enum class RecordDirection {

  /**
   * Record is an input record. This means that it reads data from the device.
   */
  INPUT,

  /**
   * Record is an output record. This means that is writes data to the device.
   */
  OUTPUT

};

namespace detail {

/**
 * Helper template struct for detecting whether a record type represents an
 * input or output record.
 *
 * The template parameter is the record type to be tested.
 */
template<typename RecordType, typename = void>
struct DetectRecordDirectionHelper {};

template<typename RecordType>
struct DetectRecordDirectionHelper<RecordType, ToVoid<decltype(std::declval<RecordType>().inp)>> {
  static constexpr RecordDirection value = RecordDirection::INPUT;
};

template<typename RecordType>
struct DetectRecordDirectionHelper<RecordType, ToVoid<decltype(std::declval<RecordType>().out)>> {
  static constexpr RecordDirection value = RecordDirection::OUTPUT;
};

} // namespace detail

/**
 * Returns the direction for the given record type. This is used to
 * automatically select the correct device support code for a record.
 */
template<typename RecordType>
constexpr RecordDirection detectRecordDirection() {
  return detail::DetectRecordDirectionHelper<RecordType>::value;
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_DIRECTION_H
