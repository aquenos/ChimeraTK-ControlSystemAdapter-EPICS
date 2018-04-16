/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2018 aquenos GmbH
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

#ifndef CHIMERATK_EPICS_RECORD_VALUE_FIELD_NAME_H
#define CHIMERATK_EPICS_RECORD_VALUE_FIELD_NAME_H

namespace ChimeraTK {
namespace EPICS {

/**
 * Name of the record's field that stores the value that is used by the device
 * support.
 */
enum RecordValueFieldName {

  /**
   * The record's RVAL field is used by the device support.
   */
  RVAL,

  /**
   * The record's VAL field is used by the device support.
   */
  VAL

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_VALUE_FIELD_NAME_H
