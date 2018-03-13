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

#include <cstdarg>
#include <cstdint>
#include <cstdio>

extern "C" {
#include <unistd.h>
}

#include <epicsThread.h>
#include <epicsTime.h>

#include "ChimeraTK/EPICS/errorPrint.h"

namespace ChimeraTK {
namespace EPICS {

static void errorPrintInternal(const char *format, const char *timeString,
    const char *threadString, std::va_list varArgs) noexcept {
  bool useAnsiSequences = ::isatty(STDERR_FILENO);
  if (useAnsiSequences) {
    // Set format to bold, red.
    std::fprintf(stderr, "\x1b[1;31m");
  }
  if (timeString) {
    std::fprintf(stderr, "%s ", timeString);
  }
  if (threadString) {
    std::fprintf(stderr, "%s ", threadString);
  }
  std::vfprintf(stderr, format, varArgs);
  if (useAnsiSequences) {
    // Reset format
    std::fprintf(stderr, "\x1b[0m");
  }
  std::fprintf(stderr, "\n");
  std::fflush(stderr);
}

void errorPrintf(const char *format, ...) noexcept {
  std::va_list varArgs;
  va_start(varArgs, format);
  errorPrintInternal(format, nullptr, nullptr, varArgs);
  va_end(varArgs);
}

void errorExtendedPrintf(const char *format, ...) noexcept {
  constexpr int bufferSize = 1024;
  char buffer[bufferSize];
  const char *timeString;
  try {
    ::epicsTime currentTime = ::epicsTime::getCurrent();
    if (currentTime.strftime(buffer, bufferSize, "%Y/%m/%d %H:%M:%S.%06f")) {
      timeString = buffer;
    } else {
      timeString = nullptr;
    }
  } catch (...) {
    timeString = nullptr;
  }
  std::va_list varArgs;
  va_start(varArgs, format);
  errorPrintInternal(format, timeString, ::epicsThreadGetNameSelf(), varArgs);
  va_end(varArgs);
}

} // namespace EPICS
} // namespace ChimeraTK
