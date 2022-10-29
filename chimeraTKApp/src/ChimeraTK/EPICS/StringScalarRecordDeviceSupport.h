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

#ifndef CHIMERATK_EPICS_STRING_SCALAR_RECORD_DEVICE_SUPPORT_H
#define CHIMERATK_EPICS_STRING_SCALAR_RECORD_DEVICE_SUPPORT_H

#include <cstring>
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
#include "ensureScanIoRequest.h"

namespace ChimeraTK {
namespace EPICS {

namespace detail {

/**
 * Helper template struct for detecting whether a record type has a SIZV field.
 *
 * The template parameter is the record type to be tested.
 */
template<typename RecordType, typename = void>
struct DetectRecordSizvFieldHelper : std::false_type {};

template<typename RecordType>
struct DetectRecordSizvFieldHelper<RecordType, ToVoid<decltype(std::declval<RecordType>().sizv)>> : std::true_type {};

/**
 * Helper structure for writing a string to a record's VAL field.
 *
 * This structure uses different code depending on whether the record has
 * a SIZV field. If it does, the value of this field is used as the max. string
 * size and the LEN field is updated with the actual length of the string. If it
 * does not, the sizeof the val field (sizeof(RecordType.VAL)) is used as the
 * max. string size.
 */
template<typename RecordType, bool HasSizvField>
struct StringRecordValueFieldHelper;

template<typename RecordType>
struct StringRecordValueFieldHelper<RecordType, true> {

  inline static void writeString(RecordType *record, std::string const &value) {
    // The max. string size (including the terminating null byte) is stored in
    // the SIZV field.
    std::strncpy(record->val, value.c_str(), record->sizv);
    // We want to ensure that the resulting string is always null-terminated,
    // even if it was truncated.
    record->val[record->sizv - 1] = '\0';
    // The record support expects the LEN field to contain the size of the
    // string including the terminating null-byte.
    record->len = std::strlen(record->val) + 1;
  }

};

template<typename RecordType>
struct StringRecordValueFieldHelper<RecordType, false> {

  inline static void writeString(RecordType *record, std::string const &value) {
    // The max. string size (including the terminating null byte) is determined
    // by the size of the VAL field (which is typically declared as char[40]).
    std::strncpy(record->val, value.c_str(), sizeof(record->val));
    // We want to ensure that the resulting string is always null-terminated,
    // even if it was truncated.
    record->val[sizeof(record->val) - 1] = '\0';
  }

};

/**
 * Base class for StringScalarRecordDeviceSupport. This class implements the
 * code that is shared by the device supports for input and output records.
 *
 * The template parameters are the record's data-structure type and a flag
 * indicating whether the record has the SIZV field.
 */
template<typename RecordType, bool HasSizvField>
class StringScalarRecordDeviceSupportTrait : public RecordDeviceSupportBase {

public:

  /**
   * Constructor. Takes a pointer to the record's data-structure and a reference
   * to the INP or OUT field.
   */
  StringScalarRecordDeviceSupportTrait(
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
    if (this->valueType != typeid(std::string)) {
      throw std::invalid_argument(
          "This record only supports process variables of type string.");
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
   * Reads the current value from the record's VAL field.
   */
  std::string readValueField() {
    // The lsi and lso as well as the stringin and stringout records ensure that
    // strings are always null-terminated.
    // We could still support strings that contain an intermediate null-byte,
    // but it is very likely that this would sometimes cause use to pick up
    // "dirt" left in memory from earlier uses, so we only use the portion of
    // the string up to the first null-byte.
    return std::string(record->val);
  }

  /**
   * Updates the record's TIME field with the specified time stamp.
   */
  void updateTimeStamp(VersionNumber const &versionNumber) {
    auto time = versionNumber.getTime();
    std::chrono::nanoseconds::rep timeInNanosecs = std::chrono::time_point_cast<std::chrono::nanoseconds>(time).
                                                   time_since_epoch().count();
    std::chrono::seconds::rep secs = timeInNanosecs / 1000000000;
    record->time.secPastEpoch =
        (secs < POSIX_TIME_AT_EPICS_EPOCH) ?
            0 : (secs - POSIX_TIME_AT_EPICS_EPOCH);
    record->time.nsec = timeInNanosecs % 1000000000;
  }

  /**
   * Writes the specified string to the record's VAL field.
   */
  void writeValueField(std::string const &value) {
    StringRecordValueFieldHelper<RecordType, HasSizvField>::writeString(
        record, value);
  }

};

} // namespace detail

/**
 * Record device support class for records that deal with (scalar) strings. At
 * the moment, these are the lsi, lso, stringin, and stringout records.
 *
 * The template parameters are the record's data-structure type, the direction
 * of the record (input or output), and a flag indicating whether the record has
 * a SIZV and LEN field (variable string size).
 */
template<typename RecordType, RecordDirection Direction = detectRecordDirection<RecordType>(), bool HasSizvField = detail::DetectRecordSizvFieldHelper<RecordType>::value>
class StringScalarRecordDeviceSupport;

// Template specialization for input records.
template<typename RecordType, bool HasSizvField>
class StringScalarRecordDeviceSupport<RecordType, RecordDirection::INPUT, HasSizvField>
    : public detail::StringScalarRecordDeviceSupportTrait<RecordType, HasSizvField> {

public:

  /**
   * Constructor. Takes a pointer to the record's data-structure.
   */
  StringScalarRecordDeviceSupport(RecordType *record)
      : detail::StringScalarRecordDeviceSupportTrait<RecordType, HasSizvField>(
          record, record->inp),
        ioIntrModeEnabled(false) {
    // Prepare the data structure needed when enabling I/O Intr mode.
    ::scanIoInit(&this->ioIntrModeScanPvt);
  }

  /**
   * Processes a request to enable or disable the I/O Intr mode.
   */
  void getInterruptInfo(int command, ::IOSCANPVT *iopvt) {
    auto pvSupport = this->template getPVSupport<std::string>();
    // A command value of 0 means enable I/O Intr mode, a value of 1 means
    // disable.
    if (command == 0) {
      if (!pvSupport->canNotify()) {
        throw std::runtime_error(
          "I/O Intr mode is not supported for this record.");
      }
      // We can safely pass this to the callback because a record device support
      // is never destroyed once successfully constructed.
      pvSupport->notify(
        [this](
            PVSupport<std::string>::SharedValue const &value,
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
          this->notifyValue = value;
          this->notifyVersionNumber = versionNumber;
          ensureScanIoRequest(this->ioIntrModeScanPvt);
        },
        [this](std::exception_ptr const &error){
          this->notifyException = error;
          ensureScanIoRequest(this->ioIntrModeScanPvt);
        });
      this->ioIntrModeEnabled = true;
    } else {
      pvSupport->cancelNotify();
      this->ioIntrModeEnabled = false;
    }
    *iopvt = this->ioIntrModeScanPvt;
  }

  /**
   * Starts a read operation, completes a read operation (depending on the
   * current state). When the record is in I/O Intr mode, this method is also
   * called to process a value that has been made available by the notify
   * callback.
   */
  void process() {
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

    auto pvSupport = this->template getPVSupport<std::string>();

    // If the record's PACT field is set, this method is called because an
    // asynchronous read completed.
    if (this->record->pact) {
      this->record->pact = false;
      if (this->readException) {
        auto tempException = this->readException;
        this->readException = std::exception_ptr();
        std::rethrow_exception(tempException);
      }
      this->writeValueField((*this->readValue)[0]);
      this->updateTimeStamp(this->readVersionNumber);
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
      this->writeValueField((*this->notifyValue)[0]);
      this->updateTimeStamp(this->notifyVersionNumber);
      pvSupport->notifyFinished();
      return;
    }

    // In all other cases, this method is called because a new value should be
    // read.
    // We can safely pass this to the callback because a record device support
    // is never destroyed once successfully constructed.
    bool immediate = pvSupport->read(
      [this](bool immediate,
          PVSupport<std::string>::SharedValue const &value,
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
        this->readValue = value;
        this->readVersionNumber = versionNumber;
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      },
      [this](bool immediate, std::exception_ptr const &error){
        this->readException = error;
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      });
    this->record->pact = true;
    if (immediate) {
      this->process();
    }
  }

private:

  /**
   * Type of a shared string value. We use shared string values instead of plain
   * strings to avoid copying the actual value twice (first when we receive it
   * from the underlying code, and a second time when putting it into the
   * record's VAL field).
   */
  using SharedStringValue = PVSupport<std::string>::SharedValue;

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
   * Version number / time stamp that belongs to notifyValue.
   */
  VersionNumber notifyVersionNumber;

  /**
   * Value that was sent with last notification.
   */
  SharedStringValue notifyValue;

  /**
   * Exception that happened during last read attempt.
   */
  std::exception_ptr readException;

  /**
   * Version number / time stamp that belongs to readValue.
   */
  VersionNumber readVersionNumber;

  /**
   * Value that was read by last read attempt.
   */
  SharedStringValue readValue;

};

// Template specialization for output records.
template<typename RecordType, bool HasSizvField>
class StringScalarRecordDeviceSupport<RecordType, RecordDirection::OUTPUT, HasSizvField>
    : public detail::StringScalarRecordDeviceSupportTrait<RecordType, HasSizvField> {

public:

  /**
   * Constructor. Takes a pointer to the record's data-structure.
   */
  StringScalarRecordDeviceSupport(RecordType *record)
      : detail::StringScalarRecordDeviceSupportTrait<RecordType, HasSizvField>(
          record, record->out) {
    // We mark the version number as invalid, so that the first notification is
    // always accepted when the the initialization fails.
    this->versionNumberValid = false;
    auto pvSupport = this->template getPVSupport<std::string>();
    try {
      auto valueTimeStampAndVersion = pvSupport->initialValue();
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
      this->writeValueField(value[0]);
      this->value = value[0];
      this->versionNumber = std::get<1>(valueTimeStampAndVersion);
      this->versionNumberValid = true;
      this->updateTimeStamp(this->versionNumber);
      // Reset the UDF flag because we now have a valid value.
      this->record->udf = 0;
      recGblResetAlarms(this->record);
    } catch (...) {
      // It might not always be possible to get an initial value, so it is not
      // an error if this fails.
    }
    // If the PV supports notifications and the nobidirectional flag has not
    // been set, we register a notification callback so that we can update the
    // record's value when it changes on the device side.
    if (!this->noBidirectional && pvSupport->canNotify()) {
      // We can safely pass this to the callback because a record device support
      // is never destroyed once successfully constructed.
      pvSupport->notify(
        [this, pvSupport](
            typename PVSupport<std::string>::SharedValue const &value,
            VersionNumber const &versionNumber) {
          // Unlike for input records we need a mutex here because processing of
          // the record may be triggered externally, so it can happen that the
          // records is processed while we are also in this callback.
          std::lock_guard<std::recursive_mutex> lock(this->mutex);
          // We only want to process the record if this update is newer than the
          // last value that we wrote. We consider a value with the same version
          // number as newer because it is coming back from the application,
          // which means that the application must already have seen the value
          // that we wrote. However, there is no need to process the record if
          // the received value is in fact the same one as the last value.
          if (!this->versionNumberValid
              || versionNumber > this->versionNumber
              || (versionNumber == this->versionNumber
                  && (this->value != (*value)[0]))) {
            bool oldNotifyPending = this->notifyPending;
            this->value = (*value)[0];
            this->versionNumber = versionNumber;
            this->notifyPending = true;
            // If another notify operation or a write operation is pending, the
            // record is going to be processed again when it has finished.
            // Otherwise, we schedule the record to be processed.
            if (this->notifyPending && !oldNotifyPending && !this->writePending) {
              ::callbackRequestProcessCallback(
                  &this->processCallback, priorityMedium, this->record);
            }
          }
          // Four output records, we do not need a strict guarantee that we see
          // all values received from the device, so we can tell the PV support
          // that the notification has finished.
          pvSupport->notifyFinished();
        },
        [pvSupport](std::exception_ptr const &error) {
          // It we receive an error notification for an output record, we cannot
          // tell whether this error precedes the last write operation (unlike a
          // value, an error is not associated with a version number), so we
          // choose to ignore it.
          pvSupport->notifyFinished();
        });
    }
  }

  /**
   * Starts or completes a write operation (depending on the current state).
   */
  void process() {
    // We have to hold a lock on the notify mutex in this function because we
    // access fields that are also accessed from the notify callback.
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    // If the record's PACT field is set, this method is called because an
    // asynchronous read completed.
    if (this->record->pact) {
      this->record->pact = false;
      this->writePending = false;
      if (this->writeException) {
        auto tempException = this->writeException;
        this->writeException = std::exception_ptr();
        // If a notification is pending, we want that notification to be
        // processed after we have signaled the failure of the last write
        // operation to the user.
        if (notifyPending) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
        std::rethrow_exception(tempException);
      }
      // If there is no pending notification, we are done. Otherwise, we process
      // that notification.
      if (!notifyPending) {
        return;
      }
    }
    // If we have received a notification, we update the record's value with the
    // received value. We do not have to check the version number because this
    // already happened before setting the notifyPending flag.
    if (this->notifyPending) {
      this->notifyPending = false;
      this->writeValueField(this->value);
      this->updateTimeStamp(this->versionNumber);
      return;
    }

    // Otherwise, this method is called because a value should be written.
    // We can safely pass this to the callback because a record device support
    // is never destroyed once successfully constructed.
    this->value = this->readValueField();
    auto pvSupport = this->template getPVSupport<std::string>();
    // We generate a new version number for the write operation. This ensures
    // that we will only accept value updates recevied from the application that
    // are newer than the value that we are writing now.
    this->versionNumber = VersionNumber();
    this->updateTimeStamp(this->versionNumber);
    bool immediate = pvSupport->write(
      std::vector<std::string>{this->value},
      this->versionNumber,
      [this](bool immediate) {
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      },
      [this](bool immediate, std::exception_ptr const &error){
        this->writeException = error;
        if (!immediate) {
          ::callbackRequestProcessCallback(
            &this->processCallback, priorityMedium, this->record);
        }
      });
    this->record->pact = true;
    this->writePending = true;
    if (immediate) {
      this->process();
    }
  }

private:

  /**
   * Mutex that is protecting access to notifyPending, value, versionNumber,
   * versionNumberValid, and writePending.
   */
  std::recursive_mutex mutex;

  /**
   * Flag indicating whether a notification has been received that has not been
   * processed yet. This field must only be accessed while holding a lock on the
   * mutex.
   */
  bool notifyPending;

  /**
   * Value that was last written to the application or that we received with the
   * last notification. This field must only be accessed while holding a lock on
   * the mutex.
   */
  std::string value;

  /**
   * Version number that is associated with the current value of the record.
   * This can either be the version number used for the last write operation or
   * the version received as part of the last notification. We use this in order
   * to decide whether a value received from the device (through a notification)
   * is older or newer than the value that we already have. This field must only
   * be accessed while holding a lock on the mutex.
   */
  VersionNumber versionNumber;

  /**
   * Flag indicating whether the version number is valid.  If it is not valid,
   * any notification that is received is accepted. This field must only be
   * accessed while holding a lock on the mutex.
   */
  bool versionNumberValid;

  /**
   * Exception that occurred while trying to write a value.
   */
  std::exception_ptr writeException;

  /**
   * Flag indicating that an asynchronous write operation is in progress.
   * Basically, this is the same flag as the PACT field of the record, but the
   * PACT field may be modifed by the record support code while this one is
   * exclusively used by the device support. This field must only be accessed
   * while holding a lock on the mutex.
   */
  bool writePending;

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_STRING_SCALAR_RECORD_DEVICE_SUPPORT_H
