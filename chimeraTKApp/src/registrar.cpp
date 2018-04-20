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

#include <ChimeraTK/ControlSystemAdapter/ApplicationBase.h>
#include <ChimeraTK/ControlSystemAdapter/PVManager.h>
#include <ChimeraTK/Utilities.h>

#include "ChimeraTK/EPICS/PVProviderRegistry.h"
#include "ChimeraTK/EPICS/errorPrint.h"

extern "C" {
#include <epicsExport.h>
#include <initHooks.h>
#include <iocsh.h>
}

using namespace ChimeraTK;
using namespace ChimeraTK::EPICS;

extern "C" {

  // Data structures needed for the iocsh chimeraTKConfigureApplication
  // function.
  static const iocshArg iocshChimeraTKConfigureApplicationArg0 = {
      "application ID", iocshArgString };
  static const iocshArg iocshChimeraTKConfigureApplicationArg1 = {
      "polling interval", iocshArgInt };
  static const iocshArg * const iocshChimeraTKConfigureApplicationArgs[] = {
      &iocshChimeraTKConfigureApplicationArg0,
      &iocshChimeraTKConfigureApplicationArg1 };
  static const iocshFuncDef iocshChimeraTKConfigureApplicationFuncDef = {
      "chimeraTKConfigureApplication", 2, iocshChimeraTKConfigureApplicationArgs };

  // Init hook that takes care of actually starting the application. This hook
  // is registered by the iocshChimeraTKConfigureApplicationFunc function.
  static void runAppInitHook(::initHookState state) noexcept {
    if (state == initHookAtIocRun) {
      try {
        ApplicationBase &application = ApplicationBase::getInstance();
        application.run();
      } catch (std::exception &e) {
        errorPrintf("Could not start the application: %s", e.what());
        return;
      } catch (...) {
        errorPrintf("Could not start the application: Unknown error.");
      }
    }
  }

  /**
   * Implementation of the iocsh chimeraTKConfigureApplication function.
   *
   * This function creates a PVManager and passes its device-part to the only
   * instance of ApplicationBase. It also registers the control-system part of
   * the PVManager with the PVProviderRegistry, using the specified name.
   */
  static void iocshChimeraTKConfigureApplicationFunc(const iocshArgBuf *args) noexcept {
    char *applicationId = args[0].sval;
    int pollingInterval = args[1].ival;
    // Verify and convert the parameters.
    if (!applicationId) {
      errorPrintf(
        "Could not configure the application: Application ID must be specified.");
      return;
    }
    if (!std::strlen(applicationId)) {
      errorPrintf(
        "Could not configure the application: Application ID must not be empty.");
      return;
    }
    if (pollingInterval <= 0) {
      // Use default value of 100 microseconds.
      pollingInterval = 100;
    }
    auto pvManagers = createPVManager();
    // We use a pointer instead of a reference. getInstance() returns a
    // reference, but if we used the reference directly, we could not have the
    // call to getInstance() inside a try-catch block.
    ApplicationBase *application = nullptr;
    try {
      application = &ApplicationBase::getInstance();
    } catch (std::exception &e) {
      errorPrintf("Could not get the application instance: %s", e.what());
      return;
    } catch (...) {
      errorPrintf(
        "Could not get the application instance.");
      return;
    }
    try {
      application->setPVManager(pvManagers.second);
      application->initialise();
    } catch (std::exception &e) {
      errorPrintf("Could not initialize the application: %s", e.what());
      return;
    } catch (...) {
      errorPrintf("Could not initialize the application: Unknown error.");
    }
    try {
      PVProviderRegistry::registerApplication(applicationId, pvManagers.first,
        pollingInterval);
    } catch (std::exception &e) {
      errorPrintf("Could not register the application: %s", e.what());
      return;
    } catch (...) {
      errorPrintf("Could not register the application: Unknown error.");
      return;
    }
    // We delay starting the application thread until the IOC is
    // started.
    ::initHookRegister(runAppInitHook);
  }

  // Data structures needed for the iocsh chimeraTKOpenAsyncDevice function.
  static const iocshArg iocshChimeraTKOpenAsyncDeviceArg0 = {
      "device ID", iocshArgString };
  static const iocshArg iocshChimeraTKOpenAsyncDeviceArg1 = {
      "device name alias", iocshArgString };
  static const iocshArg iocshChimeraTKOpenAsyncDeviceArg2 = {
      "number of I/O threads", iocshArgInt };
  static const iocshArg * const iocshChimeraTKOpenAsyncDeviceArgs[] = {
      &iocshChimeraTKOpenAsyncDeviceArg0,
      &iocshChimeraTKOpenAsyncDeviceArg1,
      &iocshChimeraTKOpenAsyncDeviceArg2 };
  static const iocshFuncDef iocshChimeraTKOpenAsyncDeviceFuncDef = {
      "chimeraTKOpenAsyncDevice", 3, iocshChimeraTKOpenAsyncDeviceArgs };

  /**
   * Implementation of the iocsh chimeraTKOpenAsyncDevice function.
   *
   * This function creates registers a ChimeraTK Device Access device with the
   * device registry. The device support operates in asynchronous mode so that
   * I/O operations that block do not affect the IOC.
   */
  static void iocshChimeraTKOpenAsyncDeviceFunc(const iocshArgBuf *args) noexcept {
    char *deviceId = args[0].sval;
    char *deviceNameAlias = args[1].sval;
    int numberOfIoThreads = args[2].ival;
    // Verify and convert the parameters.
    if (!deviceId) {
      errorPrintf(
        "Could not open the device: Device ID must be specified.");
      return;
    }
    if (!std::strlen(deviceId)) {
      errorPrintf(
        "Could not open the device: Device ID must not be empty.");
      return;
    }
    if (!deviceNameAlias) {
      errorPrintf(
        "Could not open the device: Device name alias must be specified.");
      return;
    }
    if (!std::strlen(deviceNameAlias)) {
      errorPrintf(
         "Could not open the device: Device name alias must not be empty.");
      return;
    }
    if (numberOfIoThreads <= 0) {
      errorPrintf(
        "Could not open the device: The number of I/O threads must be greater than zero.");
      return;
    }
    try {
      PVProviderRegistry::registerDevice(deviceId, deviceNameAlias,
        numberOfIoThreads);
    } catch (std::exception &e) {
      errorPrintf("Could not open the device: %s", e.what());
      return;
    } catch (...) {
      errorPrintf("Could not open the device: Unknown error.");
      return;
    }
  }

  // Data structures needed for the iocsh chimeraTKOpenSyncDevice function.
  static const iocshArg iocshChimeraTKOpenSyncDeviceArg0 = {
      "device ID", iocshArgString };
  static const iocshArg iocshChimeraTKOpenSyncDeviceArg1 = {
      "device name alias", iocshArgString };
  static const iocshArg * const iocshChimeraTKOpenSyncDeviceArgs[] = {
      &iocshChimeraTKOpenSyncDeviceArg0,
      &iocshChimeraTKOpenSyncDeviceArg1 };
  static const iocshFuncDef iocshChimeraTKOpenSyncDeviceFuncDef = {
      "chimeraTKOpenSyncDevice", 2, iocshChimeraTKOpenSyncDeviceArgs };

  /**
   * Implementation of the iocsh chimeraTKOpenSyncDevice function.
   *
   * This function creates registers a ChimeraTK Device Access device with the
   * device registry. The device support operates in synchronous mode so I/O
   * I/O operations that block will affect the IOC. For this reason, the
   * synchronous mode should only be used with device backends that known not to
   * block.
   */
  static void iocshChimeraTKOpenSyncDeviceFunc(const iocshArgBuf *args) noexcept {
    char *deviceId = args[0].sval;
    char *deviceNameAlias = args[1].sval;
    // Verify and convert the parameters.
    if (!deviceId) {
      errorPrintf(
        "Could not open the device: Device ID must be specified.");
      return;
    }
    if (!std::strlen(deviceId)) {
      errorPrintf(
        "Could not open the device: Device ID must not be empty.");
      return;
    }
    if (!deviceNameAlias) {
      errorPrintf(
        "Could not open the device: Device name alias must be specified.");
      return;
    }
    if (!std::strlen(deviceNameAlias)) {
      errorPrintf(
         "Could not open the device: Device name alias must not be empty.");
      return;
    }
    try {
      PVProviderRegistry::registerDevice(deviceId, deviceNameAlias, 0);
    } catch (std::exception &e) {
      errorPrintf("Could not open the device: %s", e.what());
      return;
    } catch (...) {
      errorPrintf("Could not open the device: Unknown error.");
      return;
    }
  }

  // Data structures needed for the iocsh chimeraTKSetDMapFilePath function.
  static const iocshArg iocshChimeraTKSetDMapFilePathArg0 = {
      "file path", iocshArgString };
  static const iocshArg * const iocshChimeraTKSetDMapFilePathArgs[] = {
      &iocshChimeraTKSetDMapFilePathArg0 };
  static const iocshFuncDef iocshChimeraTKSetDMapFilePathFuncDef = {
      "chimeraTKSetDMapFilePath", 1, iocshChimeraTKSetDMapFilePathArgs };

  /**
   * Implementation of the iocsh chimeraTKSetDMapFilePath function.
   *
   * This function sets the path to the .dmap file used by ChimeraTK. That file
   * contains definitions of device aliases, mapping them to device names and
   * register mapping files.
   */
  static void iocshChimeraTKSetDMapFilePathFunc(const iocshArgBuf *args) noexcept {
    char *dMapFilePath = args[0].sval;
    if (!dMapFilePath) {
      errorPrintf(
        "Could not set the file path: The file path must be specified.");
      return;
    }
    if (!std::strlen(dMapFilePath)) {
      errorPrintf(
        "Could not set the file path: The file path must not be empty.");
      return;
    }
    try {
      ChimeraTK::setDMapFilePath(dMapFilePath);
    } catch (std::exception &e) {
      errorPrintf("Could not set the file path: %s", e.what());
      return;
    } catch (...) {
      errorPrintf("Could not set the file path: Unknown error.");
      return;
    }
  }

  static void chimeraTKControlSystemAdapterRegistrar() {
    ::iocshRegister(&iocshChimeraTKConfigureApplicationFuncDef,
        iocshChimeraTKConfigureApplicationFunc);
    ::iocshRegister(&iocshChimeraTKOpenAsyncDeviceFuncDef,
        iocshChimeraTKOpenAsyncDeviceFunc);
    ::iocshRegister(&iocshChimeraTKOpenSyncDeviceFuncDef,
        iocshChimeraTKOpenSyncDeviceFunc);
    ::iocshRegister(&iocshChimeraTKSetDMapFilePathFuncDef,
        iocshChimeraTKSetDMapFilePathFunc);
  }

  epicsExportRegistrar(chimeraTKControlSystemAdapterRegistrar);

}
