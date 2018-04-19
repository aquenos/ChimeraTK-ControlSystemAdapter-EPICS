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

#ifndef CHIMERATK_EPICS_DEVICE_ACCESS_PV_PROVIDER_H
#define CHIMERATK_EPICS_DEVICE_ACCESS_PV_PROVIDER_H

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include <ChimeraTK/Device.h>

#include "PVProvider.h"
#include "PVSupport.h"
#include "ThreadPoolExecutor.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * PVProvider for devices using Device Access. This class effectively wraps the
 * Device from ChimeraTK Device Access.
 */
class DeviceAccessPVProvider :
    public PVProvider,
    public std::enable_shared_from_this<DeviceAccessPVProvider> {

public:

  /**
   * Type of a shared pointer to this type.
   */
  using SharedPtr = std::shared_ptr<DeviceAccessPVProvider>;

  /**
   * Creates a PV provider for the device specified by the alias name.
   *
   * This constructor opens the device and creates the specified number of pool
   * I/O threads. It throws an exception if the device cannot be opened.
   */
  DeviceAccessPVProvider(
      std::string const &deviceAliasName, int numberOfIoThreads);

  /**
   * Destroys this PV provider.
   *
   * The destuctor shuts down all I/O threads and closes the device. The
   * destructor waits until all I/O threads have actually shut down, so it may
   * block for some time.
   */
  virtual ~DeviceAccessPVProvider();

  // Declared in PVProvider.
  virtual std::type_info const &getDefaultType(
      std::string const &processVariableName) override;

protected:

  // Declared in PVProvider.
  virtual std::shared_ptr<PVSupportBase> createPVSupport(
      std::string const &processVariableName,
      std::type_info const& elementType) override;

private:

  /**
   * The DeviceAccessPVSupport class is a friend so that it can access the
   * device field and submitIoTask method.
   */
  template<typename T>
  friend class DeviceAccessPVSupport;

  /**
   * Map of member functions used for creating PV supports of different types.
   * This map is initialized by the constructor by calling
   * insertCreatePVSupportFunc() for each supported type.
   */
  std::unordered_map<std::type_index, std::shared_ptr<PVSupportBase> (DeviceAccessPVProvider::*)(std::string const &)> createPVSupportFuncs;

  /**
   * Device used by this PV provider.
   */
  Device device;

  /**
   * Thread pool used for processing all I/O requests.
   */
  ThreadPoolExecutor ioExecutor;

  /**
   * Indicates whether the PV supports for this provider works synchronously
   * (perform I/O operations in the calling thread).
   */
  bool synchronous;

  // Delete copy constructors and assignment operators.
  DeviceAccessPVProvider(DeviceAccessPVProvider const&) = delete;
  DeviceAccessPVProvider(DeviceAccessPVProvider &&) = delete;
  DeviceAccessPVProvider &operator=(DeviceAccessPVProvider const&) = delete;
  DeviceAccessPVProvider &operator=(DeviceAccessPVProvider &&) = delete;

  /**
   * Creates a process variable support for the specified process variable.
   * The returned PV support uses the element type specified as a template
   * parameter. Throws an exception if the specified element type is not
   * supported by the underlying device or if no process variable with the
   * specified name exists.
   */
  template<typename T>
  PVSupportBase::SharedPtr createPVSupportInternal(
      std::string const &processVariableName);

  /**
   * Inserts a pointer to the createPVSupportInternal(...) method of the
   * specified type into the createPVSupportFuncs map.
   */
  template<typename T>
  void insertCreatePVSupportFunc();

  /**
   * Performs I/O operations. This method is called by the polling thread
   * created by the constructor.
   */
  void runIoThread();

  /**
   * Schedule an I/O task to be run by one of the I/O threads. If there are no
   * I/O threads, the task is run immediately, right in the thread that called
   * this method.
   *
   * Returns true if the task has been run in this thread and false if it has
   * been scheduled for processing by an I/O thread.
   */
  template<typename Function>
  bool submitIoTask(Function &&f);

};

template<typename Function>
bool DeviceAccessPVProvider::submitIoTask(Function &&f) {
    if (this->synchronous) {
        f();
        return true;
    } else {
        this->ioExecutor.submitTask(std::forward<Function>(f));
        return false;
    }
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_DEVICE_ACCESS_PV_PROVIDER_H
