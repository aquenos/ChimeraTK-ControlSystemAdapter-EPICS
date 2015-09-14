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

#include "mtca4uRecordDeviceSupport.h"

using namespace mtca4u::epics;

/**
 * Prints an error message with thread and time information.
 */
static void errorPrintf(const char *format, ...) {
  const int bufferSize = 1024;
  char buffer[bufferSize];
  const char *timeString;
  try {
    ::epicsTime currentTime = ::epicsTime::getCurrent();
    if (currentTime.strftime(buffer, bufferSize, "%Y/%m/%d %H:%M:%S.%06f")) {
      timeString = buffer;
    } else {
      timeString = NULL;
    }
  } catch (...) {
    timeString = NULL;
  }
  std::va_list varArgs;
  va_start(varArgs, format);
  bool useAnsiSequences = ::isatty(STDERR_FILENO);
  if (useAnsiSequences) {
    // Set format to bold, red.
    std::fprintf(stderr, "\x1b[1;31m");
  }
  if (timeString) {
    std::fprintf(stderr, "%s ", timeString);
  }
  std::fprintf(stderr, "%s ", ::epicsThreadGetNameSelf());
  std::vfprintf(stderr, format, varArgs);
  if (useAnsiSequences) {
    // Reset format
    std::fprintf(stderr, "\x1b[0m");
  }
  std::fprintf(stderr, "\n");
  std::fflush(stderr);
  va_end(varArgs);
}

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
    RecordDeviceSupport<RecordType, IsArray, DataDirection> *deviceSupport =
        new RecordDeviceSupport<RecordType, IsArray, DataDirection>(record);
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
    aoRecord *record = static_cast<aoRecord *>(recordAsVoid);
    RecordDeviceSupport<aoRecord, false, OUTPUT> *deviceSupport =
        static_cast<RecordDeviceSupport<aoRecord, false, OUTPUT> *>(record->dpvt);
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
    RecordDeviceSupport<RecordType, IsArray, DataDirection> *deviceSupport =
        static_cast<RecordDeviceSupport<RecordType, IsArray, DataDirection> *>(record->dpvt);
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
    RecordDeviceSupport<aiRecord, false, INPUT> *deviceSupport =
        static_cast<RecordDeviceSupport<aiRecord, false, INPUT> *>(record->dpvt);
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
    RecordDeviceSupport<RecordType, IsArray, INPUT> *deviceSupport =
        static_cast<RecordDeviceSupport<RecordType, IsArray, INPUT> *>(record->dpvt);
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
} devAaiMtca4u = { 5, NULL, NULL, initRecord<aaiRecord, true, INPUT>, getIoInt<
    aaiRecord, true>, processRecord<aaiRecord, true, INPUT> };
epicsExportAddress(dset, devAaiMtca4u)
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
} devAaoMtca4u = { 5, NULL, NULL, initRecord<aaoRecord, true, OUTPUT>, NULL,
    processRecord<aaoRecord, true, OUTPUT> };
epicsExportAddress(dset, devAaoMtca4u)
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
} devAiMtca4u = { 6, NULL, NULL, initRecord<aiRecord, false, INPUT>, getIoInt<
    aiRecord, false>, processAiRecord, NULL };
epicsExportAddress(dset, devAiMtca4u)
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
} devAoMtca4u = { 6, NULL, NULL, initAoRecord,
NULL, processRecord<aoRecord, false, OUTPUT>, nullptr };
epicsExportAddress(dset, devAoMtca4u)
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
} devBiMtca4u = { 5, NULL, NULL, initRecord<biRecord, false, INPUT>, getIoInt<
    biRecord, false>, processRecord<biRecord, false, INPUT> };
epicsExportAddress(dset, devBiMtca4u)
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
} devBoMtca4u = { 5, NULL, NULL, initRecord<boRecord, false, OUTPUT>,
NULL, processRecord<boRecord, false, OUTPUT> };
epicsExportAddress(dset, devBoMtca4u)
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
} devLonginMtca4u = { 5, NULL, NULL, initRecord<longinRecord, false, INPUT>,
    getIoInt<longinRecord, false>, processRecord<longinRecord, false, INPUT> };
epicsExportAddress(dset, devLonginMtca4u)
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
} devLongoutMtca4u = { 5, NULL, NULL, initRecord<longoutRecord, false, OUTPUT>,
NULL, processRecord<longoutRecord, false, OUTPUT> };
epicsExportAddress(dset, devLongoutMtca4u)
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
} devMbbiDirectMtca4u = { 5, NULL, NULL, initRecord<mbbiDirectRecord, false,
    INPUT>, getIoInt<mbbiDirectRecord, false>, processRecord<mbbiDirectRecord,
    false, INPUT> };
epicsExportAddress(dset, devMbbiDirectMtca4u)
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
} devMbboDirectMtca4u = { 5, NULL, NULL, initRecord<mbboDirectRecord, false,
    OUTPUT>, NULL, processRecord<mbboDirectRecord, false, OUTPUT> };
epicsExportAddress(dset, devMbboDirectMtca4u)
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
} devMbbiMtca4u = { 5, NULL, NULL, initRecord<mbbiRecord, false, INPUT>,
    getIoInt<mbbiRecord, false>, processRecord<mbbiRecord, false, INPUT> };
epicsExportAddress(dset, devMbbiMtca4u)
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
} devMbboMtca4u = { 5, NULL, NULL, initRecord<mbboRecord, false, OUTPUT>,
NULL, processRecord<mbboRecord, false, OUTPUT> };
epicsExportAddress(dset, devMbboMtca4u)
;

/**
 * waveform (input) record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN read;
} devWaveformInMtca4u = { 5, NULL, NULL,
    initRecord<waveformRecord, true, INPUT>, getIoInt<waveformRecord, true>,
    processRecord<waveformRecord, true, INPUT> };
epicsExportAddress(dset, devWaveformInMtca4u)
;

/**
 * waveform (output) record type.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
} devWaveformOutMtca4u = { 5, NULL, NULL, initRecord<waveformRecord, true,
    OUTPUT>, NULL, processRecord<waveformRecord, true, OUTPUT> };
epicsExportAddress(dset, devWaveformOutMtca4u)
;

} // extern "C"
