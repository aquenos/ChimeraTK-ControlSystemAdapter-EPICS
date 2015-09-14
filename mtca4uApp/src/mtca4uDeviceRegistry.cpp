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

#include <stdexcept>

#include <boost/thread.hpp>

#include "mtca4uDeviceRegistry.h"

namespace mtca4u {
namespace epics {

// Static member variables need an instance...
DeviceRegistry::DeviceMap DeviceRegistry::deviceMap;
boost::recursive_mutex DeviceRegistry::mutex;

namespace {

/**
 * Simple class that represents the thread that processes notifications for a
 * device and schedules asynchronous processing of the respective records.
 */
class NotificationPoller {

public:
  NotificationPoller(DeviceRegistry::Device::SharedPtr device) :
      device(device) {
  }

  void operator()() {
    while (!boost::this_thread::interruption_requested()) {
      boost::chrono::microseconds::duration pollingInterval;
      {
        boost::unique_lock<boost::recursive_mutex> lock(device->mutex);
        ProcessVariable::SharedPtr nextProcessVariable;
        nextProcessVariable = device->pvManager->nextNotification();
        while (nextProcessVariable) {
          DeviceRegistry::Device::ProcessVariableSupportMap::iterator i =
              device->processVariableSupportMap.find(
                  nextProcessVariable->getName());
          if (i != device->processVariableSupportMap.end()) {
            DeviceRegistry::ProcessVariableSupport::SharedPtr pvSupport =
                i->second;
            if (pvSupport->interruptHandler
                && !pvSupport->interruptHandlingPending) {
              pvSupport->interruptHandlingPending =
                  nextProcessVariable->receive();
              if (pvSupport->interruptHandlingPending) {
                pvSupport->interruptHandler->interrupt();
              }
            }
          }
          nextProcessVariable = device->pvManager->nextNotification();
        }
        // Update the polling interval so that the thread sleeps the correct
        // period of time after releasing the mutex. We have to do this while
        // holding the mutex, because the mutex also protects the polling
        // interval field.
        pollingInterval = device->pollingInterval;
      }
      // If the notification queue is empty, we sleep for a short amount of time
      // before checking again. This also ensures that the mutex is released for
      // some time and other threads have the chance to take it.
      boost::this_thread::sleep_for(pollingInterval);
    }
  }

private:
  DeviceRegistry::Device::SharedPtr device;

};

} // anonymous namespace

DeviceRegistry::Device::Device() :
    pollingInterval(boost::chrono::microseconds::duration(100)) {
  // We want a small load factor so that the lookups (which are very
  // frequent) are fast.
  processVariableSupportMap.max_load_factor(0.5f);
}

DeviceRegistry::ProcessVariableSupport::SharedPtr DeviceRegistry::Device::createProcessVariableSupport(
    std::string const & processVariableName) {
  boost::unique_lock<boost::recursive_mutex> lock(mutex);
  ProcessVariableSupportMap::iterator i = processVariableSupportMap.find(
      processVariableName);
  if (i != processVariableSupportMap.end()) {
    throw std::runtime_error(
        "Attempt to create a second process-variable support for the same process variable. This device support does not allow more than one record to refer to the same process variable.");
  } else {
    ProcessVariableSupport::SharedPtr processVariableSupport =
        boost::make_shared<ProcessVariableSupport>();
    processVariableSupportMap.insert(
        std::make_pair(processVariableName, processVariableSupport));
    return processVariableSupport;
  }
}

void DeviceRegistry::registerDevice(std::string const & deviceName,
    ControlSystemPVManager::SharedPtr pvManager,
    int pollingIntervalInMicroSeconds) {
  if (pollingIntervalInMicroSeconds <= 0) {
    throw std::invalid_argument("Polling interval must be positive.");
  }
  if (deviceMap.find(deviceName) != deviceMap.end()) {
    throw std::invalid_argument("Specified device name is already in use.");
  }
  Device::SharedPtr device = boost::make_shared<Device>();
  device->pvManager = pvManager;
  device->pollingInterval = boost::chrono::microseconds(
      pollingIntervalInMicroSeconds);
  device->pollingThread = boost::thread(NotificationPoller(device));
  {
    boost::unique_lock<boost::recursive_mutex> lock(mutex);
    deviceMap.insert(std::make_pair(deviceName, device));
  }

}

DeviceRegistry::Device::SharedPtr DeviceRegistry::getDevice(
    std::string const & deviceName) {
  boost::unique_lock<boost::recursive_mutex> lock(mutex);
  DeviceMap::const_iterator i = deviceMap.find(deviceName);
  if (i == deviceMap.end()) {
    throw std::invalid_argument(
        "Could not find a device with the specified name.");
  }
  return i->second;
}

} // namespace epics
} // namespace mtca4u
