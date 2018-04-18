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

#ifndef CHIMERATK_EPICS_RUN_DELAYED_H
#define CHIMERATK_EPICS_RUN_DELAYED_H

#include <chrono>
#include <thread>

extern "C" {
#include <dbScan.h>
} // extern "C"

#include "ChimeraTK/EPICS/Timer.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * Calls scanIoRequest(...). If scanIoRequest returns a non-success status code,
 * it is called again later, until it succeeds.
 *
 * scanIoRequest(...) can fail for two reasons: Either the IOC is not completely
 * initialized yet (very likely) or the list of callbacks is already full
 * (less likely). In either case, it is important that we try again later,
 * because no more events are being delivered until the record is process and
 * thus the received event is acknowledged by calling the PVSupport's
 * notifyFinished() method.
 */
void ensureScanIoRequest(::IOSCANPVT ioScanPvt) {
  if (!::scanIoRequest(ioScanPvt)) {
    Timer::shared().submitDelayedTask(
      std::chrono::milliseconds(100),
      [ioScanPvt](){
        ensureScanIoRequest(ioScanPvt);
      });
  }
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_RUN_DELAYED_H
