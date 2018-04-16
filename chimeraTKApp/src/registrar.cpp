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
   * Implementation of the iocsh chimeraTKConfigureApplicationFunc function.
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

  static void chimeraTKControlSystemAdapterRegistrar() {
    ::iocshRegister(&iocshChimeraTKConfigureApplicationFuncDef,
        iocshChimeraTKConfigureApplicationFunc);
  }

  epicsExportRegistrar(chimeraTKControlSystemAdapterRegistrar);

}
