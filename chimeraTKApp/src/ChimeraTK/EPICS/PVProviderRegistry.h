/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2018-2022 aquenos GmbH
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
   * Finalizes the initialization of the PV providers.
   *
   * This calls the method of the same name on all PV providers that are
   * registered with this registry.
   *
   * This method must only be called once. Calling it more than once results in
   * in an std::logic_error being thrown.
   */
  static void finalizeInitialization();


  /**
   * Returns the PV provider for a specific application or device. This method
   * is only intended for use by the device support classes for the various
   * record types.
   */
  static PVProvider::SharedPtr getPVProvider(std::string const & name);

  /**
   * Registers a ChimeraTK Control System Adapter application. This method has
   * to be called for each application that shall be used with this device
   * support. The application name is an arbitrary name (that may not contain
   * whitespace) that is used to identify the application.
   *
   * The PV manager must be a reference to the control-system PV manager for the
   * application.
   *
   * The application name must be unique and must be different from any other
   * application name or device name that has been registered with this
   * registry.
   *
   * Calling this method after finalizeInitialization() causes an
   * std::logic_error to be thrown.
   */
  static void registerApplication(
      std::string const &appName,
      ControlSystemPVManager::SharedPtr pvManager);

  /**
   * Registers a ChimeraTK Device Access device. This method has to be called
   * for each device that shall be used with this device support.
   *
   * The first parameter is the device name, which is an arbitrary name (that
   * may not contain whitespace) that is used to identify the device.
   *
   * The second parameter is the alias of the device used by ChimeraTK Device
   * Access. Typically, this alias is defined in a .dmap file.
   *
   * The third parameter is the number of I/O threads. The PV provider
   * internally creates a fixed-size pool of threads that are used to for
   * performing I/O operations (that might potentially block). If the number of
   * threads is zero, the PVProvider operates in synchronous mode. This means
   * that all I/O operations will be performed in the thread that starts them,
   * possibly blocking this thread.
   *
   * The device name must be unique and must be different from any other
   * application name or device name that has been registered with this
   * registry.
   *
   * Calling this method after finalizeInitialization() causes an
   * std::logic_error to be thrown.
   */
  static void registerDevice(
      std::string const &devName,
      std::string const &deviceNameAlias,
      std::size_t numberOfIoThreads);

private:

  static bool finalizeInitializationCalled;
  static std::recursive_mutex mutex;
  static std::unordered_map<std::string, PVProvider::SharedPtr> pvProviders;

  // This class only has static methods, so it should not be constructed.
  PVProviderRegistry() = delete;

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_DEVICE_REGISTRY_H
