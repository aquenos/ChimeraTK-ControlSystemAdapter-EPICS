/*
 * MTCA4U for EPICS.
 *
 * Copyright 2015 aquenos GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MTCA4U_EPICS_RECORD_DEVICE_SUPPORT_H
#define MTCA4U_EPICS_RECORD_DEVICE_SUPPORT_H

#include <cstring>
#include <stdexcept>
#include <string>

#include <boost/thread.hpp>

extern "C" {
#include <aaiRecord.h>
#include <aaoRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <dbCommon.h>
#include <dbFldTypes.h>
#include <dbScan.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboDirectRecord.h>
#include <menuConvert.h>
#include <waveformRecord.h>
}

#include <string>
#include <utility>

#include "mtca4uDeviceRegistry.h"

namespace mtca4u {
namespace epics {

/**
 * Data direction of a record.
 */
enum Direction {

  /**
   * Record is an input record. This means that it reads data from the device.
   */
  INPUT,

  /**
   * Record is an output record. This means that is writes data to the device.
   */
  OUTPUT

};

/**
 * Namespace for classes and functions that are only used internally.
 */
namespace impl {

/**
 * Base functionality shared by all record device support classes.
 */
class RecordDeviceSupportTrait {

public:
  /**
   * Pointer to the device.
   */
  DeviceRegistry::Device::SharedPtr device;

  /**
   * Pointer to the process variable.
   */
  ProcessVariable::SharedPtr processVariable;

  /**
   * Pointer to the process-variable support object.
   */
  DeviceRegistry::ProcessVariableSupport::SharedPtr processVariableSupport;

  /**
   * Constructor. Takes a reference to the record's link field (typically INP or
   * OUT) that is used to determine the device name and process-variable name.
   */
  RecordDeviceSupportTrait(DBLINK const & recordLink) {
    std::pair<std::string, std::string> deviceNameAndProcessVariableName =
        parseRecordAddress(recordLink);
    device = DeviceRegistry::getDevice(deviceNameAndProcessVariableName.first);
    {
      boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
      processVariableSupport = device->createProcessVariableSupport(
          deviceNameAndProcessVariableName.second);
      processVariable = device->pvManager->getProcessVariable(
          deviceNameAndProcessVariableName.second);
    }
  }

private:
  static std::pair<std::string, std::string> parseRecordAddress(
      DBLINK const & recordAddress) {
    if (recordAddress.type != INST_IO) {
      throw std::invalid_argument("Invalid address. Maybe INP/OUT is empty?");
    }
    std::string fullString(recordAddress.value.instio.string);
    std::string::size_type firstSeparatorIndex = fullString.find_first_of(
        " \t");
    if (firstSeparatorIndex == std::string::npos) {
      throw std::invalid_argument(
          "Invalid address. No separator found that separates the device name from the process-variable name.");
    }
    std::string deviceName = fullString.substr(0, firstSeparatorIndex);
    std::string::size_type processVariableNameIndex =
        fullString.find_first_not_of(" \t", firstSeparatorIndex);
    if (processVariableNameIndex == std::string::npos) {
      throw std::invalid_argument(
          "Invalid address. Process variable name is missing.");
    }
    std::string processVariableName = fullString.substr(
        processVariableNameIndex);
    return std::make_pair(deviceName, processVariableName);
  }

};

/**
 * Base type for a helper object that allows reading from and writing to scalar
 * process variables. This type is purely virtual. The intention of this class
 * is to allow reading from or writing to a process variable without having to
 * know its exact type.
 */
struct ScalarDataAccess {

  /**
   * Reads a 32-bit signed integer, converting from the process variable's
   * actual type if needed.
   */
  virtual std::int32_t readInt32() = 0;

  /**
   * Writes a 32-bit signed integer, converting to the process variable's
   * actual type if needed.
   */
  virtual void writeInt32(std::int32_t value) = 0;

  /**
   * Reads a 32-bit unsigned integer, converting from the process variable's
   * actual type if needed.
   */
  virtual std::uint32_t readUInt32() = 0;

  /**
   * Writes a 32-bit unsigned integer, converting to the process variable's
   * actual type if needed.
   */
  virtual void writeUInt32(std::uint32_t value) = 0;

  /**
   * Reads a double, converting from the process variable's actual type if
   * needed.
   */
  virtual double readDouble() = 0;

  /**
   * Writes a double, converting to the process variable's actual type if
   * needed.
   */
  virtual void writeDouble(double value) = 0;

  /**
   * Virtual destructor. This is only defined to avoid compiler warnings.
   */
  virtual ~ScalarDataAccess() {
  }

};

/**
 * Template class implementing the {@link ScalarDataAccess} interface for the
 * various actual types that a scalar process variable can have.
 */
template<typename ValueType>
struct ScalarDataAccessImpl: public ScalarDataAccess {

  /**
   * Shared pointer to this type.
   */
  typename ProcessScalar<ValueType>::SharedPtr pv;

  /**
   * Constructor. Takes a pointer to the process variable that this data-access
   * object shall provide access to.
   */
  ScalarDataAccessImpl(ProcessVariable::SharedPtr processVariable) :
      pv(
          boost::dynamic_pointer_cast<ProcessScalar<ValueType>, ProcessVariable>(
              processVariable)) {
    if (!pv) {
      throw std::invalid_argument("Process variable is not of expected type.");
    }
  }

  std::int32_t readInt32() {
    return static_cast<std::int32_t>(pv->get());
  }

  void writeInt32(std::int32_t value) {
    pv->set(static_cast<ValueType>(value));
  }

  std::uint32_t readUInt32() {
    return static_cast<std::uint32_t>(pv->get());
  }

  void writeUInt32(std::uint32_t value) {
    pv->set(static_cast<ValueType>(value));
  }

  double readDouble() {
    return static_cast<double>(pv->get());
  }

  void writeDouble(double value) {
    pv->set(static_cast<ValueType>(value));
  }

};

/**
 * Base type for a helper object that allows reading from and writing to array
 * process variables. This type is purely virtual. The intention of this class
 * is to allow reading from or writing to a process variable without having to
 * know its exact type.
 */
struct ArrayDataAccess {

  /**
   * Reads from the process variable by copying its current content into the
   * passed array. The passed array must be of the correct type and size.
   */
  virtual void read(void *array) = 0;

  /**
   * Writes to the process variable by copying the passed array into the
   * process variable's vector. The passed array must be of the correct type and
   * size.
   */
  virtual void write(void *array) = 0;

  /**
   * Virtual destructor. This is only defined to avoid compiler warnings.
   */
  virtual ~ArrayDataAccess() {
  }

};

/**
 * Template class implementing the {@link ArrayDataAccess} interface for the
 * various actual types that an array process variable can have.
 */
template<typename ValueType>
struct ArrayDataAccessImpl: public ArrayDataAccess {
  typename ProcessArray<ValueType>::SharedPtr pv;
  std::size_t recordArraySize;

  /**
   * Constructor. Takes a pointer to the process variable that this data-access
   * object shall provide access to, the process variables expected element
   * type, and the expected number of array elements.
   *
   * If the process variable's element type does not match the expected element
   * type or if the expected number of elements does not match the actual number
   * of elements that the process variable has, an exception is thrown.
   */
  ArrayDataAccessImpl(ProcessVariable::SharedPtr processVariable,
      std::size_t recordArraySize) :
      pv(
          boost::dynamic_pointer_cast<ProcessArray<ValueType>, ProcessVariable>(
              processVariable)), recordArraySize(recordArraySize) {
    if (!pv) {
      throw std::invalid_argument("Process variable is not of expected type.");
    }
    if (pv->getConst().size() != recordArraySize) {
      throw std::runtime_error(
          "Array sizes do not match. Please check the NELM field.");
    }
  }

  void read(void *array) {
    std::vector<ValueType> const & vector = pv->getConst();
    if (recordArraySize != vector.size()) {
      throw std::runtime_error("Array sizes do not match.");
    }
    std::memcpy(array, &(vector[0]), recordArraySize);
  }

  void write(void *array) {
    std::vector<ValueType> & vector = pv->get();
    if (recordArraySize != vector.size()) {
      throw std::runtime_error("Array sizes do not match.");
    }
    std::memcpy(&(vector[0]), array, recordArraySize);
  }

};

/**
 * Template class that provides the functionality that is needed to read the
 * records link field. The actual implementation is provided by (partial)
 * specializations of this template.
 */
template<typename RecordType, Direction DataDirection>
struct RecordDeviceSupportLinkTrait {
};

/**
 * Partial template specialization for input records. This specialization reads
 * the INP field.
 */
template<typename RecordType>
struct RecordDeviceSupportLinkTrait<RecordType, INPUT> {
  static DBLINK const & getRecordAddressField(RecordType const *record) {
    return record->inp;
  }
};

/**
 * Partial template specialization for output records. This specialization reads
 * the OUT field.
 */
template<typename RecordType>
struct RecordDeviceSupportLinkTrait<RecordType, OUTPUT> {
  static DBLINK const & getRecordAddressField(RecordType const *record) {
    return record->out;
  }
};

/**
 * Template specialization for waveform records that act as outputs. This
 * specialization reads the INP field.
 */
template<>
DBLINK const & RecordDeviceSupportLinkTrait<waveformRecord, OUTPUT>::getRecordAddressField(
    waveformRecord const *record) {
  return record->inp;
}

/**
 * Base functionality shared by all record device support classes, that work
 * with scalar process variables.
 */
struct RecordDeviceSupportScalarTrait {

  /**
   * Data-access object for the actual type of the process variable.
   */
  boost::scoped_ptr<ScalarDataAccess> dataAccess;

  /**
   * Initializes the {@link #dataAccess} with an instance of the correct type.
   *
   * Throws an exception if the process variable is not a scalar or has a
   * value type that is not supported by this implementation.
   */
  void initializeDataAccess(ProcessVariable::SharedPtr processVariable) {
    if (processVariable->isArray()) {
      throw std::runtime_error(
          "This record type only supports scalar process variables, however it refers to an array process variable.");
    }
    std::type_info const & valueType = processVariable->getValueType();
    if (valueType == typeid(std::int8_t)) {
      dataAccess.reset(new ScalarDataAccessImpl<std::int8_t>(processVariable));
    } else if (valueType == typeid(std::uint8_t)) {
      dataAccess.reset(new ScalarDataAccessImpl<std::uint8_t>(processVariable));
    } else if (valueType == typeid(std::int16_t)) {
      dataAccess.reset(new ScalarDataAccessImpl<std::int16_t>(processVariable));
    } else if (valueType == typeid(std::uint16_t)) {
      dataAccess.reset(
          new ScalarDataAccessImpl<std::uint16_t>(processVariable));
    } else if (valueType == typeid(std::int32_t)) {
      dataAccess.reset(new ScalarDataAccessImpl<std::int32_t>(processVariable));
    } else if (valueType == typeid(std::uint32_t)) {
      dataAccess.reset(
          new ScalarDataAccessImpl<std::uint32_t>(processVariable));
    } else if (valueType == typeid(float)) {
      dataAccess.reset(new ScalarDataAccessImpl<float>(processVariable));
    } else if (valueType == typeid(double)) {
      dataAccess.reset(new ScalarDataAccessImpl<double>(processVariable));
    } else {
      throw std::runtime_error(
          "Encountered a process variable of an unsupported type.");
    }
  }

};

/**
 * Base functionality shared by all record device support classes, that work
 * with scalar process variables.
 */
struct RecordDeviceSupportArrayTrait {

  /**
   * Data-access object for the actual type of the process variable.
   */
  boost::scoped_ptr<ArrayDataAccess> dataAccess;

  /**
   * Initializes the {@link #dataAccess} with an instance of the correct type.
   *
   * Throws an exception if the process variable is not an array, has an
   * element type that does not match the record's element type (as specified
   * by the <code>ftvl</code> parameter), or has a size that does not match the
   * record's number of elements (as specified by the <code>nelm</code>
   * parameter).
   */
  void initializeDataAccess(ProcessVariable::SharedPtr processVariable,
      epicsEnum16 ftvl, epicsUInt32 nelm) {
    if (!processVariable->isArray()) {
      throw std::runtime_error(
          "This record type only supports array process variables, however it refers to a scalar process variable.");
    }
    std::type_info const & valueType = processVariable->getValueType();
    if (valueType == typeid(std::int8_t)) {
      dataAccess.reset(
          new ArrayDataAccessImpl<std::int8_t>(processVariable, nelm));
      if (ftvl != DBF_CHAR) {
        throw std::runtime_error(
            "Expected FTVL == CHAR in order to match the process variable type.");
      }
    } else if (valueType == typeid(std::uint8_t)) {
      dataAccess.reset(
          new ArrayDataAccessImpl<std::uint8_t>(processVariable, nelm));
      if (ftvl != DBF_UCHAR) {
        throw std::runtime_error(
            "Expected FTVL == UCHAR in order to match the process variable type.");
      }
    } else if (valueType == typeid(std::int16_t)) {
      dataAccess.reset(
          new ArrayDataAccessImpl<std::int16_t>(processVariable, nelm));
      if (ftvl != DBF_SHORT) {
        throw std::runtime_error(
            "Expected FTVL == SHORT in order to match the process variable type.");
      }
    } else if (valueType == typeid(std::uint16_t)) {
      dataAccess.reset(
          new ArrayDataAccessImpl<std::uint16_t>(processVariable, nelm));
      if (ftvl != DBF_USHORT) {
        throw std::runtime_error(
            "Expected FTVL == USHORT in order to match the process variable type.");
      }
    } else if (valueType == typeid(std::int32_t)) {
      dataAccess.reset(
          new ArrayDataAccessImpl<std::int32_t>(processVariable, nelm));
      if (ftvl != DBF_LONG) {
        throw std::runtime_error(
            "Expected FTVL == LONG in order to match the process variable type.");
      }
    } else if (valueType == typeid(std::uint32_t)) {
      dataAccess.reset(
          new ArrayDataAccessImpl<std::uint32_t>(processVariable, nelm));
      if (ftvl != DBF_ULONG) {
        throw std::runtime_error(
            "Expected FTVL == ULONG in order to match the process variable type.");
      }
    } else if (valueType == typeid(float)) {
      dataAccess.reset(new ArrayDataAccessImpl<float>(processVariable, nelm));
      if (ftvl != DBF_FLOAT) {
        throw std::runtime_error(
            "Expected FTVL == FLOAT in order to match the process variable type.");
      }
    } else if (valueType == typeid(double)) {
      dataAccess.reset(new ArrayDataAccessImpl<double>(processVariable, nelm));
      if (ftvl != DBF_DOUBLE) {
        throw std::runtime_error(
            "Expected FTVL == DOUBLE in order to match the process variable type.");
      }
    } else {
      throw std::runtime_error(
          "Encountered a process variable of an unsupported type.");
    }
  }

};

} // namespace impl

/**
 * Template class for the device support for each record type. This template
 * is empty as the actual functionality is provided by partial template
 * specializations.
 */
template<typename RecordType, bool IsArray, Direction DataDirection>
class RecordDeviceSupport {
};

/**
 * Partial template specialization for record types that work with scalar
 * process variables and act as inputs.
 */
template<typename RecordType>
class RecordDeviceSupport<RecordType, false, INPUT> : private impl::RecordDeviceSupportTrait,
    private impl::RecordDeviceSupportScalarTrait {

public:
  /**
   * Constructor. Initializes the record device support for the specified record.
   */
  RecordDeviceSupport(RecordType *record) :
      impl::RecordDeviceSupportTrait(
          impl::RecordDeviceSupportLinkTrait<RecordType, INPUT>::getRecordAddressField(
              record)), record(record), interruptHandler(
          boost::make_shared<InterruptHandlerImpl>(boost::ref(*this))) {
    initializeDataAccess(processVariable);
    scanIoInit(&ioScanPvt);
  }

  /**
   * Reads the process variable's current value into the record.
   */
  void process() {
    boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
    // If interrupt handling is pending we do not receive the process
    // variable because the current value has not been used yet.
    if (!processVariableSupport->interruptHandlingPending
        && processVariable->isReceiver()) {
      processVariable->receive();
    } else {
      processVariableSupport->interruptHandlingPending = false;
    }
    readValue();
    updateTimeStamp();
    // Reset the UDF flag because this might have been the first time that the
    // record has been processed.
    this->record->udf = 0;
    // If the record is in I/O Intr mode, we have to check whether more values
    // are pending for reception because we might not receive another
    // notification from the polling thread.
    if (processVariableSupport->interruptHandler
        && processVariable->receive()) {
      processVariableSupport->interruptHandlingPending = true;
      // Schedule another processing of the record.
      scanIoRequest(ioScanPvt);
    }
  }

  /**
   * Processes a request to enable or disable the I/O Intr mode.
   */
  void getInterruptInfo(int command, IOSCANPVT *iopvt) {
    boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
    if (command == 0) {
      // Register interrupt handler
      if (!processVariable->isReceiver()) {
        // By setting iopvt to null, we can signal that I/O Intr mode is not
        // supported for this record.
        *iopvt = NULL;
        return;
      }
      processVariableSupport->interruptHandler = interruptHandler;
      // The interrupt pending flag should be false, however we reset it to be
      // extra sure.
      processVariableSupport->interruptHandlingPending = false;
    } else {
      // Remove interrupt handler
      processVariableSupport->interruptHandler =
          DeviceRegistry::ProcessVariableInterruptHandler::SharedPtr();
    }
    *iopvt = this->ioScanPvt;
  }

  /**
   * Returns a flag indicating whether conversions should be applied to a raw
   * value that has been read into the RVAL field of the record
   * (<code>false</code>) or whether the value has been directly read into the
   * VAL field and thus no conversions should be applied (<code>true</code>).
   * This flag is only relevant for ai records.
   */
  bool isNoConvert() {
    return false;
  }

private:
  struct InterruptHandlerImpl: DeviceRegistry::ProcessVariableInterruptHandler {

    // It is okay to use a reference because the record support is never
    // destroyed once it has been constructed successfully and the the interrupt
    // handler is never used before the record support has been constructed.
    RecordDeviceSupport &recordDeviceSupport;

    InterruptHandlerImpl(RecordDeviceSupport &recordDeviceSupport) :
        recordDeviceSupport(recordDeviceSupport) {
    }

    void interrupt() {
      // If another interrupt is pending, we do not receive a new value but wait
      // for this interrupt to be processed first.
      if (!recordDeviceSupport.processVariableSupport->interruptHandlingPending) {
        recordDeviceSupport.processVariable->receive();
        // Schedule processing of the record.
        scanIoRequest(recordDeviceSupport.ioScanPvt);
      }
    }

  };

  RecordType *record;
  IOSCANPVT ioScanPvt;
  DeviceRegistry::ProcessVariableInterruptHandler::SharedPtr interruptHandler;

  void updateTimeStamp() {
    TimeStamp timeStamp = processVariable->getTimeStamp();
    record->time.secPastEpoch =
        (timeStamp.seconds < POSIX_TIME_AT_EPICS_EPOCH) ?
            0 : (timeStamp.seconds - POSIX_TIME_AT_EPICS_EPOCH);
    record->time.nsec = timeStamp.nanoSeconds;
  }

  void readValue();

};

/**
 * Template specialization for the ai record. The device support for the ai
 * record reads flating point numbers directly into the VAL field while integer
 * numbers are read into the RVAL field so that conversions can be applied.
 */
template<>
bool RecordDeviceSupport<aiRecord, false, INPUT>::isNoConvert() {
  return processVariable->getValueType() == typeid(float)
      || processVariable->getValueType() == typeid(double);
}

template<>
void RecordDeviceSupport<aiRecord, false, INPUT>::readValue() {
  if (isNoConvert()) {
    // For floating point variables we read the value directly into VAL and do
    // not attempt to convert (return value 0).
    record->val = dataAccess->readDouble();
  } else {
    // For integer types, we write into RVAL so that conversions can be applied.
    record->rval = dataAccess->readInt32();
  }
}

template<>
void RecordDeviceSupport<biRecord, false, INPUT>::readValue() {
  record->rval = dataAccess->readUInt32();
}

template<>
void RecordDeviceSupport<longinRecord, false, INPUT>::readValue() {
  record->val = dataAccess->readInt32();
}

template<>
void RecordDeviceSupport<mbbiRecord, false, INPUT>::readValue() {
  record->rval = dataAccess->readUInt32();
}

template<>
void RecordDeviceSupport<mbbiDirectRecord, false, INPUT>::readValue() {
  record->rval = dataAccess->readUInt32();
}

/**
 * Partial template specialization for record types that work with scalar
 * process variables and act as outputs.
 */
template<typename RecordType>
class RecordDeviceSupport<RecordType, false, OUTPUT> : private impl::RecordDeviceSupportTrait,
    private impl::RecordDeviceSupportScalarTrait {

public:
  /**
   * Constructor. Initializes the record device support for the specified record.
   */
  RecordDeviceSupport(RecordType *record) :
      impl::RecordDeviceSupportTrait(
          impl::RecordDeviceSupportLinkTrait<RecordType, OUTPUT>::getRecordAddressField(
              record)), record(record) {
    initializeDataAccess(processVariable);
    {
      boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
      if (!processVariable->isSender()) {
        throw std::runtime_error(
            "The process variable that is used with an output record must be a sender.");
      }
      // Initialize value from device.
      readValue();
      updateTimeStamp();
      // Reset the UDF flag because we now have a valid value.
      this->record->udf = 0;
    }
  }

  /**
   * Writes the record's current value into the process variable.
   */
  void process() {
    boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
    writeValue();
    processVariable->send();
  }

  /**
   * Returns a flag indicating whether conversions should be applied to a raw
   * value that has been read into the RVAL field of the record
   * (<code>false</code>) or whether the value has been directly read into the
   * VAL field and thus no conversions should be applied (<code>true</code>).
   * This flag is only relevant for ai records.
   */
  bool isNoConvert() {
    return false;
  }

private:
  RecordType *record;

  void updateTimeStamp() {
    TimeStamp timeStamp = processVariable->getTimeStamp();
    record->time.secPastEpoch =
        (timeStamp.seconds < POSIX_TIME_AT_EPICS_EPOCH) ?
            0 : (timeStamp.seconds - POSIX_TIME_AT_EPICS_EPOCH);
    record->time.nsec = timeStamp.nanoSeconds;
  }

  void readValue();
  void writeValue();

};

/**
 * Template specialization for the ai record. The device support for the ai
 * record reads flating point numbers directly into the VAL field while integer
 * numbers are read into the RVAL field so that conversions can be applied.
 */
template<>
bool RecordDeviceSupport<aoRecord, false, OUTPUT>::isNoConvert() {
  return processVariable->getValueType() == typeid(float)
      || processVariable->getValueType() == typeid(double);
}

template<>
void RecordDeviceSupport<aoRecord, false, OUTPUT>::readValue() {
  if (isNoConvert()) {
    // For floating point variables we read the value directly into VAL and do
    // not attempt to convert (return value 0).
    record->val = dataAccess->readDouble();
  } else {
    // For integer types, we write into RVAL so that conversions can be applied.
    record->rval = dataAccess->readInt32();
  }
}

template<>
void RecordDeviceSupport<aoRecord, false, OUTPUT>::writeValue() {
  if (isNoConvert()) {
    // For floating point variables we use VAL directly.
    dataAccess->writeDouble(record->val);
  } else {
    // For integer types, we use RVAL so that conversions can be applied.
    dataAccess->writeInt32(record->rval);
  }
}

template<>
void RecordDeviceSupport<boRecord, false, OUTPUT>::readValue() {
  record->rval = dataAccess->readUInt32();
}

template<>
void RecordDeviceSupport<boRecord, false, OUTPUT>::writeValue() {
  dataAccess->writeUInt32(record->rval);
}

template<>
void RecordDeviceSupport<longoutRecord, false, OUTPUT>::readValue() {
  record->val = dataAccess->readInt32();
}

template<>
void RecordDeviceSupport<longoutRecord, false, OUTPUT>::writeValue() {
  dataAccess->writeInt32(record->val);
}

template<>
void RecordDeviceSupport<mbboRecord, false, OUTPUT>::readValue() {
  record->rval = dataAccess->readUInt32();
}

template<>
void RecordDeviceSupport<mbboRecord, false, OUTPUT>::writeValue() {
  dataAccess->writeUInt32(record->rval);
}

template<>
void RecordDeviceSupport<mbboDirectRecord, false, OUTPUT>::readValue() {
  record->rval = dataAccess->readUInt32();
}

template<>
void RecordDeviceSupport<mbboDirectRecord, false, OUTPUT>::writeValue() {
  dataAccess->writeUInt32(record->rval);
}

/**
 * Partial template specialization for record types that work with array process
 * variables and act as inputs.
 */
template<typename RecordType>
class RecordDeviceSupport<RecordType, true, INPUT> : private impl::RecordDeviceSupportTrait,
    private impl::RecordDeviceSupportArrayTrait {

public:
  /**
   * Constructor. Initializes the record device support for the specified record.
   */
  RecordDeviceSupport(RecordType *record) :
      impl::RecordDeviceSupportTrait(
          impl::RecordDeviceSupportLinkTrait<RecordType, INPUT>::getRecordAddressField(
              record)), record(record), interruptHandler(
          boost::make_shared<InterruptHandlerImpl>(boost::ref(*this))) {
    initializeDataAccess(processVariable, record->ftvl, record->nelm);
    scanIoInit(&ioScanPvt);
  }

  /**
   * Reads the process variable's current value into the record.
   */
  void process() {
    boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
    // If interrupt handling is pending we do not receive the process
    // variable because the current value has not been used yet.
    if (!processVariableSupport->interruptHandlingPending
        && processVariable->isReceiver()) {
      processVariable->receive();
    } else {
      processVariableSupport->interruptHandlingPending = false;
    }
    dataAccess->read(record->bptr);
    record->nord = record->nelm;
    updateTimeStamp();
    // Reset the UDF flag because this might have been the first time that the
    // record has been processed.
    this->record->udf = 0;
    // If the record is in I/O Intr mode, we have to check whether more values
    // are pending for reception because we might not receive another
    // notification from the polling thread.
    if (processVariableSupport->interruptHandler
        && processVariable->receive()) {
      processVariableSupport->interruptHandlingPending = true;
      // Schedule another processing of the record.
      scanIoRequest(ioScanPvt);
    }
  }

  /**
   * Processes a request to enable or disable the I/O Intr mode.
   */
  void getInterruptInfo(int command, IOSCANPVT *iopvt) {
    boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
    if (command == 0) {
      // Register interrupt handler
      if (!processVariable->isReceiver()) {
        // By setting iopvt to null, we can signal that I/O Intr mode is not
        // supported for this record.
        *iopvt = NULL;
        return;
      }
      processVariableSupport->interruptHandler = interruptHandler;
      // The interrupt pending flag should be false, however we reset it to be
      // extra sure.
      processVariableSupport->interruptHandlingPending = false;
    } else {
      // Remove interrupt handler
      processVariableSupport->interruptHandler =
          DeviceRegistry::ProcessVariableInterruptHandler::SharedPtr();
    }
    *iopvt = this->ioScanPvt;
  }

private:
  struct InterruptHandlerImpl: DeviceRegistry::ProcessVariableInterruptHandler {

    // It is okay to use a reference because the record support is never
    // destroyed once it has been constructed successfully and the the interrupt
    // handler is never used before the record support has been constructed.
    RecordDeviceSupport &recordSupport;

    InterruptHandlerImpl(RecordDeviceSupport &recordSupport) :
        recordSupport(recordSupport) {
    }

    void interrupt() {
      // If another interrupt is pending, we do not receive a new value but wait
      // for this interrupt to be processed first.
      if (!recordSupport.processVariableSupport->interruptHandlingPending) {
        recordSupport.processVariable->receive();
        // Schedule processing of the record.
        scanIoRequest(recordSupport.ioScanPvt);
      }
    }

  };

  RecordType *record;
  IOSCANPVT ioScanPvt;
  DeviceRegistry::ProcessVariableInterruptHandler::SharedPtr interruptHandler;

  void updateTimeStamp() {
    TimeStamp timeStamp = processVariable->getTimeStamp();
    record->time.secPastEpoch =
        (timeStamp.seconds < POSIX_TIME_AT_EPICS_EPOCH) ?
            0 : (timeStamp.seconds - POSIX_TIME_AT_EPICS_EPOCH);
    record->time.nsec = timeStamp.nanoSeconds;
  }

};

/**
 * Partial template specialization for record types that work with scalar
 * process variables and act as outputs.
 */
template<typename RecordType>
class RecordDeviceSupport<RecordType, true, OUTPUT> : private impl::RecordDeviceSupportTrait,
    private impl::RecordDeviceSupportArrayTrait {

public:
  /**
   * Constructor. Initializes the record device support for the specified record.
   */
  RecordDeviceSupport(RecordType *record) :
      impl::RecordDeviceSupportTrait(
          impl::RecordDeviceSupportLinkTrait<RecordType, OUTPUT>::getRecordAddressField(
              record)), record(record) {
    initializeDataAccess(processVariable, record->ftvl, record->nelm);
    {
      boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
      if (!processVariable->isSender()) {
        throw std::runtime_error(
            "The process variable that is used with an output record must be a sender.");
      }
      dataAccess->read(record->bptr);
      record->nord = record->nelm;
      updateTimeStamp();
      // Reset the UDF flag because we now have a valid value.
      this->record->udf = 0;
    }
  }

  /**
   * Writes the record's current value into the process variable.
   */
  void process() {
    boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
    dataAccess->write(record->bptr);
    processVariable->send();
    record->nord = record->nelm;
  }

private:
  RecordType *record;

  void updateTimeStamp() {
    TimeStamp timeStamp = processVariable->getTimeStamp();
    record->time.secPastEpoch =
        (timeStamp.seconds < POSIX_TIME_AT_EPICS_EPOCH) ?
            0 : (timeStamp.seconds - POSIX_TIME_AT_EPICS_EPOCH);
    record->time.nsec = timeStamp.nanoSeconds;
  }

};

} // namespace epics
} // namespace mtca4u

#endif // MTCA4U_EPICS_RECORD_DEVICE_SUPPORT_H
