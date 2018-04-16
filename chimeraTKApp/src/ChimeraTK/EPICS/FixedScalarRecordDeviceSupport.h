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

#ifndef CHIMERATK_EPICS_FIXED_SCALAR_RECORD_DEVICE_SUPPORT_H
#define CHIMERATK_EPICS_FIXED_SCALAR_RECORD_DEVICE_SUPPORT_H

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

#include "RecordDeviceSupportBase.h"
#include "RecordDirection.h"
#include "RecordValueFieldName.h"

namespace ChimeraTK {
namespace EPICS {

namespace detail {

/**
 * Helper structure for getting a records VAL or RVAL field, depending on a
 * template parameter.
 */
template<typename RecordType, RecordValueFieldName ValueFieldName>
struct RecordValueFieldHelper;

template<typename RecordType>
struct RecordValueFieldHelper<RecordType, RecordValueFieldName::RVAL> {

  using RecordValueType = decltype(std::declval<RecordType>().rval);

  inline static typename std::add_lvalue_reference<RecordValueType>::type getValueField(
      RecordType *record) {
    return record->rval;
  }

};

template<typename RecordType>
struct RecordValueFieldHelper<RecordType, RecordValueFieldName::VAL> {

  using RecordValueType = decltype(std::declval<RecordType>().val);

  inline static typename std::add_lvalue_reference<RecordValueType>::type getValueField(
      RecordType *record) {
    return record->val;
  }

};

/**
 * Base class for FixedScalarRecordDeviceSupport. This class implements the code
 * that is shared by the device supports for input and output records.
 *
 * The template parameters are the record's data-structure type and the name of
 * the record's field that is supposed to be accessed by the device support
 * (RVAL or VAL).
 */
template<typename RecordType, RecordValueFieldName ValueFieldName>
class FixedScalarRecordDeviceSupportTrait : public RecordDeviceSupportBase {

public:

  /**
   * Type of the record's RVAL or VAL field.
   */
  using RecordValueType =
    typename RecordValueFieldHelper<RecordType, ValueFieldName>::RecordValueType;

  /**
   * Constructor. Takes a pointer to the record's data-structure and a reference
   * to the INP or OUT field.
   */
  FixedScalarRecordDeviceSupportTrait(
      RecordType *record, ::DBLINK const &linkField)
      : RecordDeviceSupportBase(RecordAddress::parse(linkField)),
        record(record) {
    // The ChimeraTK Control System Adapter and ChimeraTK Device Access ensure
    // that the number of elements does not change after initialization. For
    // this reason, it is sufficient when we check the number of elements once
    // during initialization.
    std::size_t numberOfElements = this->pvSupport->getNumberOfElements();
    if (numberOfElements != 1) {
      std::ostringstream oss;
      oss << "Process variable has " << numberOfElements
          << " elements, but the record needs exactly one element.";
      throw std::invalid_argument(oss.str());
    }
  }

protected:

  /**
   * Auxilliary data strucuture needed by callbackRequestProcessCallback.
   */
  ::CALLBACK processCallback;

  /**
   * Pointer to the record structure.
   */
  RecordType *record;

  /**
   * Returns a reference to the record's RVAL or VAL field.
   */
  inline typename std::add_lvalue_reference<RecordValueType>::type const &getValueField() {
    return RecordValueFieldHelper<RecordType, ValueFieldName>::getValueField(
      this->record);
  }

  /**
   * Updates the record's TIME field with the specified time stamp.
   */
  void updateTimeStamp(TimeStamp const &timeStamp) {
    record->time.secPastEpoch =
        (timeStamp.seconds < POSIX_TIME_AT_EPICS_EPOCH) ?
            0 : (timeStamp.seconds - POSIX_TIME_AT_EPICS_EPOCH);
    record->time.nsec = timeStamp.nanoSeconds;
  }

};

} // namespace detail

/**
 * Record device support class for records that always use a fixed, scalar type
 * when reading or writing.
 *
 * The template parameters are the record's data-structure type, the direction
 * of the record (input or output), and the name of the record's field that is
 * supposed to be accessed by the device support (RVAL or VAL).
 */
template<typename RecordType, RecordDirection Direction, RecordValueFieldName ValueFieldName>
class FixedScalarRecordDeviceSupport;

// Template specialization for input records.
template<typename RecordType, RecordValueFieldName ValueFieldName>
class FixedScalarRecordDeviceSupport<RecordType, RecordDirection::INPUT, ValueFieldName>
    : public detail::FixedScalarRecordDeviceSupportTrait<RecordType, ValueFieldName> {

public:

  /**
   * Type of the record's RVAL or VAL field.
   */
  using RecordValueType =
    typename detail::FixedScalarRecordDeviceSupportTrait<RecordType, ValueFieldName>::RecordValueType;

  /**
   * Constructor. Takes a pointer to the record's data-structure.
   */
  FixedScalarRecordDeviceSupport(RecordType *record)
      : detail::FixedScalarRecordDeviceSupportTrait<RecordType, ValueFieldName>(
          record, record->inp),
        ioIntrModeEnabled(false) {
    // Prepare the data structure needed when enabling I/O Intr mode.
    ::scanIoInit(&this->ioIntrModeScanPvt);
  }

  /**
   * Processes a request to enable or disable the I/O Intr mode.
   */
  void getInterruptInfo(int command, ::IOSCANPVT *iopvt) {
    this->template callForValueType<CallGetInterruptInfoInternal>(
      this, command, iopvt);
  }

  /**
   * Starts a read operation, completes a read operation (depending on the
   * current state). When the record is in I/O Intr mode, this method is also
   * called to process a value that has been made available by the notify
   * callback.
   */
  void process() {
    this->template callForValueType<CallProcessInternal>(this);
  }

private:

  /**
   * Helper template for calling the right instantiation of
   * getInterruptInfoInternal for the current value type.
   */
  template<typename T>
  struct CallGetInterruptInfoInternal {
    void operator()(
        FixedScalarRecordDeviceSupport *obj, int command, ::IOSCANPVT *iopvt) {
      obj->template getInterruptInfoInternal<T>(command, iopvt);
    }
  };

  /**
   * Helper template for calling the right instantiation of processInternal for
   * the current value type.
   */
  template<typename T>
  struct CallProcessInternal {
    void operator()(FixedScalarRecordDeviceSupport *obj) {
      obj->template processInternal<T>();
    }
  };

  /**
   * Flag indicating whether the record has been set to I/O Intr mode.
   */
  bool ioIntrModeEnabled;

  /**
   * Datastructure internally needed by EPICS to handle the I/O Intr mode.
   * This data structure is initialized using scanIoInit() and later used
   * by getInterruptInfo(...).
   */
  ::IOSCANPVT ioIntrModeScanPvt;

  /**
   * Exception that was sent with last notification.
   */
  std::exception_ptr notifyException;

  /**
   * Time stamp that belongs to notifyValue.
   */
  TimeStamp notifyTimeStamp;

  /**
   * Value that was sent with last notification.
   */
  RecordValueType notifyValue;

  /**
   * Exception that happened during last read attempt.
   */
  std::exception_ptr readException;

  /**
   * Time stamp that belongs to readValue.
   */
  TimeStamp readTimeStamp;

  /**
   * Value that was read by last read attempt.
   */
  RecordValueType readValue;

  /**
   * Internal implementation of getInterruptInfo(...). This is a template that
   * is instantiated for each supported element type. The calling method ensures
   * that the version for the current element type is called.
   */
  template<typename T>
  void getInterruptInfoInternal(int command, ::IOSCANPVT *iopvt) {
    auto pvSupport = this->template getPVSupport<T>();
    // A command value of 0 means enable I/O Intr mode, a value of 0 means
    // disable.
    if (command == 0) {
      // We can safely pass this to the callback because a record device support
      // is never destroyed once successfully constructed.
      pvSupport->notify(
        [this](
            typename PVSupport<T>::SharedValue const &value,
            TimeStamp const &timeStamp,
            VersionNumber const &versionNumber) {
          // We already checked the number of elements of the PV in the
          // constructor, so this check should always succeed. However, if
          // something is changed in ChimeraTK Device Access or the Control
          // System Adapter, this simple test can save us from serious trouble.
          if (value->size() != 1) {
            std::ostringstream oss;
            oss << "Process variable has " << value->size()
                << " elements, but the record needs exactly one element.";
            throw std::logic_error(oss.str());
          }
          this->notifyValue = static_cast<RecordValueType>((*value)[0]);
          this->notifyTimeStamp = timeStamp;
          ::scanIoRequest(this->ioIntrModeScanPvt);
        },
        [this](std::exception_ptr const& error){
          this->notifyException = error;
          ::scanIoRequest(this->ioIntrModeScanPvt);
        });
      this->ioIntrModeEnabled = true;
    } else {
      pvSupport->cancelNotify();
      this->ioIntrModeEnabled = false;
    }
    *iopvt = this->ioIntrModeScanPvt;
  }

  /**
   * Internal implementation of process(...). This is a template that is
   * instantiated for each supported element type. The calling method ensures
   * that the version for the current element type is called.
   */
  template<typename T>
  void processInternal() {
    // The queue used by callbackRequestProcessCallback internally uses a lock.
    // This means that the changes that we make asynchronously are visible to
    // the thread calling this method at a later point in time. The same applies
    // to the I/O Intr mode: scanIoRequest internally uses a callback that also
    // uses a lock.
    // This only leaves two corner cases: Switching from regular mode to
    // I/O Intr mode and the other way round. In both cases, it could happen
    // that the notify callback overwrites the value previously written by the
    // read callback, but before the record is processed again.
    // We avoid this problem by using separate variables for the value and
    // exception written by a read callback and the value and exception written
    // by a notify callback.
    // The notify callback is not called again before we have acknowledged that
    // we are finished with processing, so we do not have to worry about a
    // second notification overwriting the value of an earlier notification
    // before processing of the earlier value has completed.

    auto pvSupport = this->template getPVSupport<T>();

    // If the record's PACT field is set, this method is called because an
    // asynchronous read completed.
    if (this->record->pact) {
      this->record->pact = false;
      if (this->readException) {
        auto tempException = this->readException;
        this->readException = std::exception_ptr();
        std::rethrow_exception(tempException);
      }
      this->getValueField() = this->readValue;
      this->updateTimeStamp(this->readTimeStamp);
      return;
    }

    // If the ioIntrModeEnabled flag is set, this method is called because our
    // notify callback requested the record to be processed.
    if (this->ioIntrModeEnabled) {
      if (this->notifyException) {
        auto tempException = this->notifyException;
        this->notifyException = std::exception_ptr();
        pvSupport->notifyFinished();
        std::rethrow_exception(tempException);
      }
      this->getValueField() = this->notifyValue;
      this->updateTimeStamp(this->notifyTimeStamp);
      pvSupport->notifyFinished();
      return;
    }

    // In all other cases, this method is called because a new value should be
    // read.
    // We can safely pass this to the callback because a record device support
    // is never destroyed once successfully constructed.
    bool immediate = pvSupport->read(
      [this](bool immediate,
          typename PVSupport<T>::SharedValue const &value,
          TimeStamp const &timeStamp,
          VersionNumber const &versionNumber) {
        // We already checked the number of elements of the PV in the
        // constructor, so this check should always succeed. However, if
        // something is changed in ChimeraTK Device Access or the Control System
        // Adapter, this simple test can save us from serious trouble.
        if (value->size() != 1) {
          std::ostringstream oss;
          oss << "Process variable has " << value->size()
              << " elements, but the record needs exactly one element.";
          throw std::logic_error(oss.str());
        }
        this->readValue = static_cast<RecordValueType>((*value)[0]);
        this->readTimeStamp = timeStamp;
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      },
      [this](bool immediate, std::exception_ptr const& error){
        this->readException = error;
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      });
    this->record->pact = true;
    if (immediate) {
      this->template processInternal<T>();
    }
  }

};

// Template specialization for output records.
template<typename RecordType, RecordValueFieldName ValueFieldName>
class FixedScalarRecordDeviceSupport<RecordType, RecordDirection::OUTPUT, ValueFieldName>
    : public detail::FixedScalarRecordDeviceSupportTrait<RecordType, ValueFieldName> {

public:

  /**
   * Type of the record's RVAL or VAL field.
   */
  using RecordValueType =
    typename detail::FixedScalarRecordDeviceSupportTrait<RecordType, ValueFieldName>::RecordValueType;

  /**
   * Constructor. Takes a pointer to the record's data-structure.
   */
  FixedScalarRecordDeviceSupport(RecordType *record)
      : detail::FixedScalarRecordDeviceSupportTrait<RecordType, ValueFieldName>(
          record, record->out) {
    this->template callForValueType<CallInitializeValue>(this);
  }

  /**
   * Starts or completes a write operation (depending on the current state).
   */
  void process() {
    this->template callForValueType<CallProcessInternal>(this);
  }

private:

  /**
   * Helper template for calling the right instantiation of initializedValue for
   * the current value type.
   */
  template<typename T>
  struct CallInitializeValue {
    void operator()(FixedScalarRecordDeviceSupport *obj) {
      obj->template initializeValue<T>();
    }
  };

  /**
   * Helper template for calling the right instantiation of processInternal for
   * the current value type.
   */
  template<typename T>
  struct CallProcessInternal {
    void operator()(FixedScalarRecordDeviceSupport *obj) {
      obj->template processInternal<T>();
    }
  };

  /**
   * Exception that occurred while trying to write a value.
   */
  std::exception_ptr writeException;

  /**
   * Tries to initialize the record's value with the initial value of the
   * underlying process variable.
   */
  template<typename T>
  void initializeValue() {
    try {
      auto valueTimeStampAndVersion =
        this->template getPVSupport<T>()->initialValue();
      auto &value = std::get<0>(valueTimeStampAndVersion);
      // We already checked the number of elements of the PV in the constructor,
      // so this check should always succeed. However, if something is changed
      // in ChimeraTK Device Access or the Control System Adapter, this simple
      // test can save us from serious trouble.
      if (value.size() != 1) {
        std::ostringstream oss;
        oss << "Process variable has " << value.size()
            << " elements, but the record needs exactly one element.";
        throw std::logic_error(oss.str());
      }
      this->getValueField() = static_cast<RecordValueType>(value[0]);
      // Reset the UDF flag because we now have a valid value.
      this->record->udf = 0;
    } catch (...) {
      // It might not always be possible to get an initial value, so it is not
      // an error if this fails.
    }
  }

  /**
   * Internal implementation of getInterruptInfo(...). This is a template that
   * is instantiated for each supported element type. The calling method ensures
   * that the version for the current element type is called.
   */
  template<typename T>
  void processInternal() {
    // If the record's PACT field is set, this method is called because an
    // asynchronous read completed.
    if (this->record->pact) {
      this->record->pact = false;
      if (this->writeException) {
        auto tempException = this->writeException;
        this->writeException = std::exception_ptr();
        std::rethrow_exception(tempException);
      }
      return;
    }

    // Otherwise, this method is called because a value should be written.
    // We can safely pass this to the callback because a record device support
    // is never destroyed once successfully constructed.
    auto pvSupport = this->template getPVSupport<T>();
    bool immediate = pvSupport->write(
      std::vector<T>(1, static_cast<T>(this->getValueField())),
      [this](bool immediate) {
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      },
      [this](bool immediate, std::exception_ptr const& error){
        this->writeException = error;
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      });
    this->record->pact = true;
    if (immediate) {
      this->template processInternal<T>();
    }
  }

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_FIXED_SCALAR_RECORD_DEVICE_SUPPORT_H
