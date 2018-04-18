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

#include <cstdarg>

extern "C" {
#include <unistd.h>
}

#include <epicsThread.h>
#include <epicsTime.h>

extern "C" {
#include <devSup.h>
#include <epicsExport.h>
}

#include "ChimeraTK/EPICS/RecordDeviceSupport.h"
#include "ChimeraTK/EPICS/errorPrint.h"

using namespace ChimeraTK::EPICS;

namespace {

/**
 * Returns the status code that initRecord should return on success. Normally,
 * this is 0, but it may be two for records that support disabling conversion
 * from RVAL to VAL.
 */
template<typename RecordType>
inline long getInitSuccessStatusCode(RecordType *record) {
  return 0;
}

template<>
inline long getInitSuccessStatusCode<::aoRecord>(::aoRecord *record) {
  RecordDeviceSupport<::aoRecord> *deviceSupport =
      static_cast<RecordDeviceSupport<::aoRecord> *>(record->dpvt);
  return deviceSupport->isNoConvert() ? 2 : 0;
}

/**
 * Returns the status code that processRecord should return on success.
 * Normally, this is 0, but it may be two for records that support disabling
 * conversion from RVAL to VAL.
 */
template<typename RecordType>
inline long getProcessSuccessStatusCode(RecordType *record) {
  return 0;
}

template<>
inline long getProcessSuccessStatusCode<::aiRecord>(aiRecord *record) {
  RecordDeviceSupport<::aiRecord> *deviceSupport =
      static_cast<RecordDeviceSupport<::aiRecord> *>(record->dpvt);
  return deviceSupport->isNoConvert() ? 2 : 0;
}

/**
 * Template function for the I/O interrupt action. This function is called when
 * a record is switch to or from the I/O Intr scan mode. It is responsible for
 * registering or clearing the interrupt handler.
 */
template<typename RecordType>
long getInterruptInfo(int command, void *recordAsVoid, IOSCANPVT *iopvt) {
  if (!recordAsVoid) {
    errorPrintf(
        "Record processing failed: Pointer to record structure is null.");
    return -1;
  }
  RecordType *record = static_cast<RecordType *>(recordAsVoid);
  try {
    RecordDeviceSupport<RecordType> *deviceSupport =
        static_cast<RecordDeviceSupport<RecordType> *>(record->dpvt);
    if (!deviceSupport) {
      throw std::runtime_error(
          "Pointer to device support data structure is null.");
    }
    deviceSupport->getInterruptInfo(command, iopvt);
  } catch (std::exception const & e) {
    errorPrintf("%s Record processing failed: %s", record->name, e.what());
    return -1;
  } catch (...) {
    errorPrintf("%s Record processing failed: Unknown error.", record->name);
    return -1;
  }
  return 0;
}

/**
 * Template function for initializing the device support for a record.
 */
template<typename RecordType>
long initRecord(void *recordAsVoid) {
  if (!recordAsVoid) {
    errorPrintf(
        "Record initialization failed: Pointer to record structure is null.");
    return -1;
  }
  RecordType *record = static_cast<RecordType *>(recordAsVoid);
  try {
    RecordDeviceSupport<RecordType> *deviceSupport =
        new RecordDeviceSupport<RecordType>(record);
    record->dpvt = deviceSupport;
  } catch (std::exception const & e) {
    errorPrintf("%s Record initialization failed: %s", record->name, e.what());
    return -1;
  } catch (...) {
    errorPrintf("%s Record initialization failed: Unknown error.",
        record->name);
    return -1;
  }
  return getInitSuccessStatusCode<RecordType>(record);
}

/**
 * Template function for processing (reading or writing) a record.
 */
template<typename RecordType>
long processRecord(void *recordAsVoid) {
  if (!recordAsVoid) {
    errorPrintf(
        "Record processing failed: Pointer to record structure is null.");
    return -1;
  }
  RecordType *record = static_cast<RecordType *>(recordAsVoid);
  try {
    RecordDeviceSupport<RecordType> *deviceSupport =
        static_cast<RecordDeviceSupport<RecordType> *>(record->dpvt);
    if (!deviceSupport) {
      throw std::runtime_error(
          "Pointer to device support data structure is null.");
    }
    deviceSupport->process();
  } catch (std::exception const & e) {
    errorPrintf("%s Record processing failed: %s", record->name, e.what());
    return -1;
  } catch (...) {
    errorPrintf("%s Record processing failed: Unknown error.", record->name);
    return -1;
  }
  return getProcessSuccessStatusCode<RecordType>(record);
}

/**
 * Type alias for the get_ioint_info functions. These functions have a slightly
 * different signature than the other functions, even though the definition in
 * the structures in the record header files might indicate something else.
 */
typedef long (*DEVSUPFUN_GET_IOINT_INFO)(int, void *, ::IOSCANPVT *);

/**
 * Helper template structure for getting a pointer to the getInterruptInfo
 * function. This function can only be compiled for certain records (those for
 * which the device support class has a getInterruptInfo method), so we have to
 * use a null pointer for all other records.
 *
 * We use a struct with a method instead of a function template to work around
 * the fact that function template partial specializations are not allowed.
 */
template<typename RecordType, bool = RecordDeviceSupportTraits<RecordType>::hasGetInterruptInfo>
struct GetInterruptInfoFunc {

  constexpr DEVSUPFUN_GET_IOINT_INFO operator()() const {
    return nullptr;
  }

};

template<typename RecordType>
struct GetInterruptInfoFunc<RecordType, true> {

  constexpr DEVSUPFUN_GET_IOINT_INFO operator()() const {
    return getInterruptInfo<RecordType>;
  }

};

/**
 * Device support structure as expected by most record types. The notable
 * exceptions are the ai and ao records, where the device support structure
 * has an additional field.
 */
typedef struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN process;
} DeviceSupportStruct;

template<typename RecordType>
constexpr DeviceSupportStruct deviceSupportStruct() {
  return {5, nullptr, nullptr, initRecord<RecordType>,
      GetInterruptInfoFunc<RecordType>()(), processRecord<RecordType>};
}

} // anonymous namespace

extern "C" {

/**
 * aai record type.
 */
auto devAaiChimeraTK = deviceSupportStruct<::aaiRecord>();
epicsExportAddress(dset, devAaiChimeraTK);

/**
 * aao record type.
 */
auto devAaoChimeraTK = deviceSupportStruct<::aaoRecord>();
epicsExportAddress(dset, devAaoChimeraTK);

/**
 * ai record type.  This record type expects an additional field
 * (special_linconv) in the device support structure, so we cannot use the usual
 * template function.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
  DEVSUPFUN special_linconv;
} devAiChimeraTK = {6, nullptr, nullptr, initRecord<::aiRecord>,
    getInterruptInfo<::aiRecord>, processRecord<::aiRecord>, nullptr};
epicsExportAddress(dset, devAiChimeraTK);

/**
 * ao record type.  This record type expects an additional field
 * (special_linconv) in the device support structure, so we cannot use the usual
 * template function.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
  DEVSUPFUN special_linconv;
} devAoChimeraTK = {6, nullptr, nullptr, initRecord<::aoRecord>, nullptr,
    processRecord<::aoRecord>, nullptr};
epicsExportAddress(dset, devAoChimeraTK);

/**
 * bi record type.
 */
auto devBiChimeraTK = deviceSupportStruct<::biRecord>();
epicsExportAddress(dset, devBiChimeraTK);

/**
 * bo record type.
 */
auto devBoChimeraTK = deviceSupportStruct<::boRecord>();
epicsExportAddress(dset, devBoChimeraTK);

/**
 * longin record type.
 */
auto devLonginChimeraTK = deviceSupportStruct<::longinRecord>();
epicsExportAddress(dset, devLonginChimeraTK);

/**
 * longout record type.
 */
auto devLongoutChimeraTK = deviceSupportStruct<::longoutRecord>();
epicsExportAddress(dset, devLongoutChimeraTK);

/**
 * mbbi record type.
 */
auto devMbbiChimeraTK = deviceSupportStruct<::mbbiRecord>();
epicsExportAddress(dset, devMbbiChimeraTK);

/**
 * mbbiDirect record type.
 */
auto devMbbiDirectChimeraTK = deviceSupportStruct<::mbbiDirectRecord>();
epicsExportAddress(dset, devMbbiDirectChimeraTK);

/**
 * mbbo record type.
 */
auto devMbboChimeraTK = deviceSupportStruct<::mbboRecord>();
epicsExportAddress(dset, devMbboChimeraTK);

/**
 * mbboDirect record type.
 */
auto devMbboDirectChimeraTK = deviceSupportStruct<::mbboDirectRecord>();
epicsExportAddress(dset, devMbboDirectChimeraTK);

} // extern "C"
