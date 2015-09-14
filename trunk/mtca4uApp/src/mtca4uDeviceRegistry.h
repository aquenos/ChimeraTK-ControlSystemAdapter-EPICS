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

#ifndef MTCA4U_EPICS_DEVICE_REGISTRY_H
#define MTCA4U_EPICS_DEVICE_REGISTRY_H

#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>

#include <ControlSystemPVManager.h>

namespace mtca4u {
namespace epics {

/**
 * Device registry for MTCA4U devices. Each MTCA4U device has to be registered
 * with this registry using a unique name. This name is then used to refer to
 * the device from an EPICS record.
 */
class DeviceRegistry {

public:
  /**
   * Interface for an interrupt handler. This interface is implemented by the
   * device support for input records, so that they can be notified (and
   * processed) when new data is available for their respective process
   * variables.
   */
  struct ProcessVariableInterruptHandler {

    /**
     * Shared pointer to this type.
     */
    typedef boost::shared_ptr<ProcessVariableInterruptHandler> SharedPtr;

    /**
     * Called when new data is available for the process variable.
     */
    virtual void interrupt() = 0;

  };

  /**
   * Data structure associated with each process variable. This data structure
   * stores a pointer to the interrupt handler (if one is registered) and the
   * current interrupt state.
   */
  struct ProcessVariableSupport {

    /**
     * Shared pointer to this type.
     */
    typedef boost::shared_ptr<ProcessVariableSupport> SharedPtr;

    /**
     * Default constructor. Initialized the interrupt handling pending flag with
     * <code>false</code> and the interrupt handler with a null pointer.
     */
    ProcessVariableSupport() :
        interruptHandlingPending(false) {
    }

    /**
     * Pointer to the interrupt handler for the process variable.
     */
    ProcessVariableInterruptHandler::SharedPtr interruptHandler;

    /**
     * Flag indicating whether interrupt handling is pending. If
     * <code>true</code> new data has been received for the process variable,
     * but the associated record has not been processed yet.
     */
    bool interruptHandlingPending;

  };

  /**
   * Data structure associated with each device. This data structure provides
   * the PV manager, a map storing the {@link ProcessVariableSupport} objects,
   * and the global mutex that must be locked before using the PV manager or one
   * of the process-variable support data-structures.
   */
  struct Device {

    /**
     * Shared pointer to this type.
     */
    typedef boost::shared_ptr<Device> SharedPtr;

    /**
     * Type of the map that stores the process-variable support objects.
     */
    typedef boost::unordered_map<std::string, ProcessVariableSupport::SharedPtr> ProcessVariableSupportMap;

    /**
     * Default constructor.
     */
    Device();

    /**
     * Creates and returns the process-variable support object for the specified
     * process variable. Throws an exception if a process-variable support
     * object for the specified process variable has already been created
     * earlier.
     */
    ProcessVariableSupport::SharedPtr createProcessVariableSupport(
        std::string const & processVariableName);

    /**
     * Mutex that protects all access to the device and it's PV manager.
     */
    boost::recursive_mutex mutex;

    /**
     * PV manager that provides access to the device's process variables.
     */
    ControlSystemPVManager::SharedPtr pvManager;

    /**
     * Map storing the process-variable support objects.
     */
    ProcessVariableSupportMap processVariableSupportMap;

    /**
     * Thread that periodically checks whether new values are available for
     * process variables and schedules their asynchronous processing (if the
     * corresponding record has been set to I/O Intr mode).
     */
    boost::thread pollingThread;

    /**
     * Interval for which the polling thread sleeps if there are no pending
     * notifications.
     */
    boost::chrono::microseconds pollingInterval;

  };

  /**
   * Registers a device. This method has to be called for each device that
   * shall be used with this device support. The device name is an arbitrary
   * name (that may not contain whitespace) that is used to identify the device.
   * The PV manager must be a reference to the control-system PV manager for the
   * device. The optional polling interval (default value 100) specifies the
   * number of microseconds that the polling thread sleeps, when no
   * notifications about new values are pending.
   */
  static void registerDevice(std::string const & deviceName,
      ControlSystemPVManager::SharedPtr pvManager,
      int pollingIntervalInMicroSeconds = 100);

  /**
   * Returns the device data-structure for a specific device. This method is
   * only intended for use by the device support classes for the various record
   * types.
   */
  static Device::SharedPtr getDevice(std::string const & deviceName);

private:
  typedef boost::unordered_map<std::string, Device::SharedPtr> DeviceMap;
  static DeviceMap deviceMap;
  static boost::recursive_mutex mutex;

};

} // namespace epics
} // namespace mtca4u

#endif // MTCA4U_EPICS_DEVICE_REGISTRY_H
