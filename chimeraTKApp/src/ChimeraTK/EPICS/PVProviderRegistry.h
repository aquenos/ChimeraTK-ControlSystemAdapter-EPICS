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

#ifndef CHIMERATK_EPICS_PV_PROVIDER_REGISTRY_H
#define CHIMERATK_EPICS_PV_PROVIDER_REGISTRY_H

#include <mutex>
#include <unordered_map>

#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>

#include "PVProvider.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * Registry for instances of PVProvider. These instances can either represent
 * ChimeraTK Control System Adapter applications or ChimeraTK Device Access
 * devices.
 */
class PVProviderRegistry {

public:

  /**
   * Returns the PV provider for a specific application or device. This method
   * is only intended for use by the device support classes for the various
   * record types.
   */
  static PVProvider::SharedPtr getPVProvider(std::string const & name);

  /**
   * Registers a ChimeraTK ControlSystemAdapter application. This method has to
   * be called for each application that shall be used with this device support.
   * The application name is an arbitrary name (that may not contain whitespace)
   * that is used to identify the application.

   * The PV manager must be a reference to the control-system PV manager for the
   * application. The optional polling interval (default value 100) specifies the
   * number of microseconds that the polling thread sleeps, when no notifications
   * about new values are pending.
   *
   * The application name must be unique and must be different from any other
   * application name or name identifying a ChimeraTK Device Access device that
   * has been registered with this registry.
   */
  static void registerApplication(
      std::string const & appName,
      ControlSystemPVManager::SharedPtr pvManager,
      int pollingIntervalInMicroSeconds = 100);

private:

  static std::recursive_mutex mutex;
  static std::unordered_map<std::string, PVProvider::SharedPtr> pvProviders;

  // This class only has static methods, so it should not be constructed.
  PVProviderRegistry() = delete;

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_DEVICE_REGISTRY_H
