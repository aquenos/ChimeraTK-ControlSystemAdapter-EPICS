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

/**
 * Template function for initializing the device support for a record.
 */
template<typename RecordType, bool IsArray, Direction DataDirection>
static long initRecord(void *recordAsVoid) {
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
    return 0;
  } catch (std::exception const & e) {
    errorPrintf("%s Record initialization failed: %s", record->name, e.what());
    return -1;
  } catch (...) {
    errorPrintf("%s Record initialization failed: Unknown error.",
        record->name);
    return -1;
  }
}

/**
 * Initialization function for the device support for the ao record.
 */
static long initAoRecord(void *recordAsVoid) {
  long status = initRecord<aoRecord, false, OUTPUT>(recordAsVoid);
  if (status) {
    return status;
  } else {
    aoRecord *record = static_cast<::aoRecord *>(recordAsVoid);
    RecordDeviceSupport<::aoRecord> *deviceSupport =
        static_cast<RecordDeviceSupport<::aoRecord> *>(record->dpvt);
    return deviceSupport->isNoConvert() ? 2 : 0;
  }
}

/**
 * Template function for processing (reading or writing) a record.
 */
template<typename RecordType, bool IsArray, Direction DataDirection>
static long processRecord(void *recordAsVoid) {
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
  return 0;
}

/**
 * Updates the value of an ai record by reading from the corresponding device.
 */
static long processAiRecord(void *recordAsVoid) {
  long status = processRecord<aiRecord, false, INPUT>(recordAsVoid);
  if (status) {
    return status;
  } else {
    aiRecord *record = static_cast<aiRecord *>(recordAsVoid);
    RecordDeviceSupport<::aiRecord> *deviceSupport =
        static_cast<RecordDeviceSupport<::aiRecord> *>(record->dpvt);
    return deviceSupport->isNoConvert() ? 2 : 0;
  }
}

/**
 * Template function for the I/O interrupt action. This function is called when
 * a record is switch to or from the I/O Intr scan mode. It is responsible for
 * registering or clearing the interrupt handler.
 */
template<typename RecordType, bool IsArray>
static long getIoInt(int command, void *recordAsVoid, IOSCANPVT *iopvt) {
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

extern "C" {

/**
 * Type alias for the get_ioint_info functions. These functions have a slightly
 * different signature than the other functions, even though the definition in
 * the structures in the record header files might indicate something else.
 */
typedef long (*DEVSUPFUN_GET_IOINT_INFO)(int, void *, IOSCANPVT *);

/**
 * aai record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
} devAaiChimeraTK = { 5, NULL, NULL, initRecord<aaiRecord, true, INPUT>,
    getIoInt<aaiRecord, true>, processRecord<aaiRecord, true, INPUT> };
epicsExportAddress(dset, devAaiChimeraTK)
;

/**
 * aao record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
} devAaoChimeraTK = { 5, NULL, NULL, initRecord<aaoRecord, true, OUTPUT>, NULL,
    processRecord<aaoRecord, true, OUTPUT> };
epicsExportAddress(dset, devAaoChimeraTK)
;

/**
 * ai record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
  DEVSUPFUN special_linconv;
} devAiChimeraTK = { 6, NULL, NULL, initRecord<aiRecord, false, INPUT>,
    getIoInt<aiRecord, false>, processAiRecord, NULL };
epicsExportAddress(dset, devAiChimeraTK)
;

/**
 * ao record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
  DEVSUPFUN special_linconv;
} devAoChimeraTK = { 6, NULL, NULL, initAoRecord,
NULL, processRecord<aoRecord, false, OUTPUT>, nullptr };
epicsExportAddress(dset, devAoChimeraTK)
;

/**
 * bi record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
} devBiChimeraTK = { 5, NULL, NULL, initRecord<biRecord, false, INPUT>,
    getIoInt<biRecord, false>, processRecord<biRecord, false, INPUT> };
epicsExportAddress(dset, devBiChimeraTK)
;

/**
 * bo record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
} devBoChimeraTK = { 5, NULL, NULL, initRecord<boRecord, false, OUTPUT>,
NULL, processRecord<boRecord, false, OUTPUT> };
epicsExportAddress(dset, devBoChimeraTK)
;

/**
 * longin record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
} devLonginChimeraTK = { 5, NULL, NULL, initRecord<longinRecord, false, INPUT>,
    getIoInt<longinRecord, false>, processRecord<longinRecord, false, INPUT> };
epicsExportAddress(dset, devLonginChimeraTK)
;

/**
 * longout record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
} devLongoutChimeraTK = { 5, NULL, NULL,
    initRecord<longoutRecord, false, OUTPUT>,
    NULL, processRecord<longoutRecord, false, OUTPUT> };
epicsExportAddress(dset, devLongoutChimeraTK)
;

/**
 * mbbiDirect record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
} devMbbiDirectChimeraTK = { 5, NULL, NULL, initRecord<mbbiDirectRecord, false,
    INPUT>, getIoInt<mbbiDirectRecord, false>, processRecord<mbbiDirectRecord,
    false, INPUT> };
epicsExportAddress(dset, devMbbiDirectChimeraTK)
;

/**
 * mbboDirect record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
} devMbboDirectChimeraTK = { 5, NULL, NULL, initRecord<mbboDirectRecord, false,
    OUTPUT>, NULL, processRecord<mbboDirectRecord, false, OUTPUT> };
epicsExportAddress(dset, devMbboDirectChimeraTK)
;

/**
 * mbbi record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
} devMbbiChimeraTK = { 5, NULL, NULL, initRecord<mbbiRecord, false, INPUT>,
    getIoInt<mbbiRecord, false>, processRecord<mbbiRecord, false, INPUT> };
epicsExportAddress(dset, devMbbiChimeraTK)
;

/**
 * mbbo record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
} devMbboChimeraTK = { 5, NULL, NULL, initRecord<mbboRecord, false, OUTPUT>,
NULL, processRecord<mbboRecord, false, OUTPUT> };
epicsExportAddress(dset, devMbboChimeraTK)
;

} // extern "C"
