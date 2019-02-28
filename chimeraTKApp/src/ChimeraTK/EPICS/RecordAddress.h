
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

#ifndef CHIMERATK_EPICS_RECORD_ADDRESS_H
#define CHIMERATK_EPICS_RECORD_ADDRESS_H

#include <stdexcept>
#include <string>
#include <typeinfo>

extern "C" {
#include <dbLink.h>
} // extern "C"

namespace ChimeraTK {
namespace EPICS {

/**
 * Record address for the ChimeraTK device support.
 */
class RecordAddress {

public:

  /**
   * Creates a record address using the specified parameters.
   *
   * Typically, this constructor is not called directly. Instead, the
   * parse(::DBLINK address) method is used for parsing the content of a
   * record's link field and generating an address object from it.
   */
  inline RecordAddress(
      std::string const &appOrDevName,
      std::string const &pvName,
      std::type_info const &valueType, bool valueTypeValid,
      bool noBidirectional)
      : appOrDevName(appOrDevName), noBidirectional(noBidirectional),
        pvName(pvName), valueType(valueType), valueTypeValid(valueTypeValid) {
  }

  /**
   * Returns the application or device name. This is the name used when looking
   * up the PVProvider.
   */
  inline std::string const &getApplicationOrDeviceName() const {
    return appOrDevName;
  }

  /**
   * Returns the name of the process variable.
   */
  inline std::string const &getProcessVariableName() const {
    return pvName;
  }

  /**
   * Returns the expected value type. Throws an exception if hasValueType()
   * returns false.
   */
  inline std::type_info const &getValueType() const {
    if (!valueTypeValid) {
      throw std::logic_error(
        "The getValueType() method must only be called if hasValueType() returns true.");
    }
    return valueType;
  }

  /**
   * Tells whether this record address specifies a value type.
   */
  inline bool hasValueType() const {
    return valueTypeValid;
  }

  /**
   * Tells whether the "nobidirectional" flag is set. If this flag is set,
   * output records shall not be updated when the value changes inside the
   * device, even if such updates are supported for the process variable.
   */
  inline bool isNoBidirectional() const {
    return noBidirectional;
  }

  /**
   * Parses the contents of a record's link field and returns the corresponding
   * address object.
   *
   * This method is called by the record-specific device-support code.
   */
  static RecordAddress parse(::DBLINK const &addressField);

private:

  std::string const appOrDevName;
  bool noBidirectional;
  std::string const pvName;
  std::type_info const &valueType;
  bool const valueTypeValid;

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RECORD_ADDRESS_H
