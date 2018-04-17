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

#ifndef CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_PROVIDER_H
#define CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_PROVIDER_H

#include <chrono>
#include <condition_variable>
#include <forward_list>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <typeindex>
#include <unordered_map>

#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>

#include "ControlSystemAdapterSharedPVSupportFwdDecl.h"
#include "PVProvider.h"
#include "PVSupport.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * PVProvider for applications using the Control System Adapter. This class
 * effectively wraps the ControlSystemPVManager for the application.
 */
class ControlSystemAdapterPVProvider :
    public PVProvider,
    public std::enable_shared_from_this<ControlSystemAdapterPVProvider> {

public:

  /**
   * Type of a shared pointer to this type.
   */
  using SharedPtr = std::shared_ptr<ControlSystemAdapterPVProvider>;

  /**
   * Creates a PV provider for the specified PV manager, using the specified
   * polling interval. Only one PV provider must be created for each PV manager
   * and the PV manager must not be used by other code.
   */
  ControlSystemAdapterPVProvider(
      ControlSystemPVManager::SharedPtr const & pvManager,
      std::chrono::microseconds pollingInterval);

  /**
   * Destroys this PV provider. The destructor shuts down the polling thread and
   * only returns after that thread has exited.
   */
  virtual ~ControlSystemAdapterPVProvider();

  // Declared in PVProvider.
  virtual std::type_info const &getDefaultType(
      std::string const &processVariableName) override;

protected:

  // Declared in PVProvider.
  virtual PVSupportBase::SharedPtr createPVSupport(
      std::string const &processVariableName,
      std::type_info const& elementType) override;

private:

  /**
   * The ControlSystemAdapterSharedPVSupport is a friend so that it can call
   * scheduleCallNotify(...).
   */
  template <typename T>
  friend class ControlSystemAdapterSharedPVSupport;

  /**
   * Map of member functions used for creating PV supports of different types.
   * This map is initialized by the constructor by calling
   * insertCreatePVSupportFunc() for each supported type.
   */
  std::unordered_map<std::type_index, std::shared_ptr<PVSupportBase> (ControlSystemAdapterPVProvider::*)(std::string const &)> createPVSupportFuncs;

  /**
   * Mutex protecting access to the PVManager and all other shared resources in
   * this object and its associated PV supports.
   */
  std::recursive_mutex mutex;

  /**
   * List of shared PV support instances for which doNotify() shall be called.
   * This list is filled by scheduleCallNotify(...) and processed by
   * runPollingThread().
   */
  std::forward_list<std::shared_ptr<ControlSystemAdapterSharedPVSupportBase>> pendingCallNotify;

  /**
   * Time to wait between polling for value notifications. The polling thread
   * waits when there is no more work to do and waits for the specified amount
   * of time before checking whether new events are available.
   */
  std::chrono::microseconds const pollingInterval;

  /**
   * Thread responsible for polling the PV manager for new events and calling
   * the shared PV supports' doNotify() methods.
   */
  std::thread pollingThread;

  /**
   * Condition variable used by the polling thread. When sleeping, the polling
   * thread watis on this condition variable, making it possible to wake the
   * thread up.
   */
  std::condition_variable_any pollingThreadCv;

  /**
   * Tells whether the polling thread should shut down. This flag is set by the
   * destructor in order to make sure that the polling thread quits before this
   * object is destroyed.
   */
  bool pollingThreadShutdownRequested;

  /**
   * PV manager used to access the process variables.
   */
  ControlSystemPVManager::SharedPtr pvManager;

  /**
   * Shared PV support instances created by this provider. This provider only
   * keeps weak references to the PV supports so that PV supports that are not
   * needed any longer are destroyed.
   */
  std::unordered_map<std::string, std::weak_ptr<ControlSystemAdapterSharedPVSupportBase>> sharedPVSupports;

  // Delete copy constructors and assignment operators.
  ControlSystemAdapterPVProvider(ControlSystemAdapterPVProvider const&) = delete;
  ControlSystemAdapterPVProvider(ControlSystemAdapterPVProvider &&) = delete;
  ControlSystemAdapterPVProvider &operator=(ControlSystemAdapterPVProvider const&) = delete;
  ControlSystemAdapterPVProvider &operator=(ControlSystemAdapterPVProvider &&) = delete;

  /**
   * Creates a process variable support for the specified process variable.
   * The returned PV support uses the element type specified as a template
   * parameter. Throws an exception if the specified element type does not match
   * the type used by the underlying ProcessArray or if no process variable with
   * the specified name exists.
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
   * Implements the notification logic. This method is called by the polling
   * thread created by the constructor.
   */
  void runPollingThread();

  /**
   * Schedules the passed PV support's doNotify() method to be called by the
   * polling thread.
   *
   * The code calling this method must hold a lock on the mutex.
   */
  void scheduleCallNotify(std::shared_ptr<ControlSystemAdapterSharedPVSupportBase> pvSupport);

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_PROVIDER_H
