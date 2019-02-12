/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2018-2019 aquenos GmbH
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
#include <forward_list>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/cppext/future_queue.hpp>

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
   * Creates a PV provider for the specified PV manager. Only one PV provider
   * must be created for each PV manager and the PV manager must not be used
   * by other code.
   */
  ControlSystemAdapterPVProvider(
      ControlSystemPVManager::SharedPtr const & pvManager);

  /**
   * Destroys this PV provider. The destructor shuts down the notification
   * thread and only returns after that thread has exited.
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
   * Thread responsible for waiting on PV updates and calling the shared PV
   * supports' doNotify() methods.
   */
  std::thread notificationThread;

  /**
   * Tells whether the notification thread should shut down. This flag is set by
   * the destructor in order to make sure that the notification thread quits
   * before this object is destroyed.
   */
  bool notificationThreadShutdownRequested;

  /**
   * List of shared PV support instances for which doNotify() shall be called.
   * This list is filled by scheduleCallNotify(...) and processed by
   * runNotificationThread().
   */
  std::forward_list<std::shared_ptr<ControlSystemAdapterSharedPVSupportBase>> pendingCallNotify;

  /**
   * PV manager used to access the process variables.
   */
  ControlSystemPVManager::SharedPtr pvManager;

  /**
   * Vector holding the notification queues for all process variables. The last
   * element in the vector is a queue that is only used for waking up the
   * notification thread when we have to.
   *
   * The indices into this vector are identical to the the indices into the
   * pvsForNotification vector.
   */
  std::vector<cppext::future_queue<void>> pvNotificationQueues;

  /**
   * Vector keeping a reference to each process variable that supports
   * notifications.
   */
  std::vector<ProcessVariable::SharedPtr> pvsForNotification;

  /**
   * Vector keeping a reference to the PV support for each process variable in
   * pvsForNotification. This vector uses exactly the same indices as
   * pvsForNotification.
   */
  std::vector<std::weak_ptr<ControlSystemAdapterSharedPVSupportBase>> sharedPVSupportsByIndex;

  /**
   * Shared PV support instances created by this provider. This provider only
   * keeps weak references to the PV supports so that PV supports that are not
   * needed any longer are destroyed.
   */
  std::unordered_map<std::string, std::weak_ptr<ControlSystemAdapterSharedPVSupportBase>> sharedPVSupports;

  /**
   * Special future queue that is used to wake up the notification thread when
   * it is waiting for a new notification.
   */
  cppext::future_queue<void> wakeUpQueue;

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
   * Implements the notification logic. This method is called by the
   * notification thread created by the constructor.
   */
  void runNotificationThread();

  /**
   * Schedules the passed PV support's doNotify() method to be called by the
   * notification thread, if there are pending notifications. The PV support
   * uses this method to notify the PV provider that it has finished processing
   * a notification and is ready to receive the next one.
   *
   * The code calling this method must hold a lock on the mutex.
   */
  void scheduleCallNotify(std::shared_ptr<ControlSystemAdapterSharedPVSupportBase> pvSupport);

  /**
   * Wakes the notification thread up.
   *
   * The code calling this method must hold a lock on the mutex.
   */
  void wakeUpNotificationThread();

};

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_CONTROL_SYSTEM_ADAPTER_PV_PROVIDER_H
