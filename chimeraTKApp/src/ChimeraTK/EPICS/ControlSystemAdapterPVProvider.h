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
#include <condition_variable>
#include <forward_list>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/ControlSystemAdapter/UnidirectionalProcessArray.h>
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
      std::type_info const &elementType) override;

private:

  /**
   * The ControlSystemAdapterSharedPVSupport is a friend so that it can call
   * wakeUpNotificationThread().
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
   * Condition variable used by the notification thread. When sleeping, without
   * waiting for a notification, the thread waits on this condition variable,
   * making it possible to wake the thread up. As code wanting to wake up the
   * notification thread cannot know whether it is waiting this condition
   * variable or waiting for the next notification, code wanting to wake up the
   * thread also has to write to the wakeUpPVSender.
   */
  std::condition_variable_any notificationThreadCv;

  /**
   * Tells whether the notification thread should shut down. This flag is set by
   * the destructor in order to make sure that the notification thread quits
   * before this object is destroyed.
   */
  bool notificationThreadShutdownRequested;

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
   * Tasks that have been submitted to be executed in the notfication thread,
   * but have not been run yet.
   */
  std::queue<std::function<void()>> tasks;

  /**
   * PV used to wake up the notification thread when it is waiting for a new
   * notification. In this case, the thread can be woken up by writing to this
   * PV. As code wanting to wake up the notification thread cannot know whether
   * the thread is waiting for the next notification or waiting for some other
   * event, code wanting to wake up the thread also has to notify the
   * notificationThreadCv condition variable in addition to writing to this PV.
   */
  ProcessArray<int>::SharedPtr wakeUpPV;

  // Delete copy constructors and assignment operators.
  ControlSystemAdapterPVProvider(ControlSystemAdapterPVProvider const &) = delete;
  ControlSystemAdapterPVProvider(ControlSystemAdapterPVProvider &&) = delete;
  ControlSystemAdapterPVProvider &operator=(ControlSystemAdapterPVProvider const &) = delete;
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
   * Runs a task inside the notification thread. This is primarily intended for
   * use by the PV supports so that they can run a task for which they know that
   * it will not interfer with the notification process. The tasks are run
   * before running regular notifications.
   */
  void runInNotificationThread(std::function<void()> const &task);

  /**
   * Implements the notification logic. This method is called by the
   * notification thread created by the constructor.
   */
  void runNotificationThread();

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
