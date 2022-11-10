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

#include <stdexcept>

#include "ChimeraTK/EPICS/ControlSystemAdapterPVProvider.h"
#include "ChimeraTK/EPICS/DeviceAccessPVProviderImpl.h"

#include "ChimeraTK/EPICS/PVProviderRegistry.h"

namespace ChimeraTK {
namespace EPICS {

void PVProviderRegistry::finalizeInitialization() {
  {
    std::lock_guard<std::recursive_mutex> lock(PVProviderRegistry::mutex);
    if (finalizeInitializationCalled) {
      throw std::logic_error(
          "PVProviderRegistry::finalizeInitialization has already been "
          "called.");
    }
    finalizeInitializationCalled = true;
  }
  // After setting the finalizeInitializtionCalled flag, no more PV providers
  // can be registered, so accessing the PV providers map without holding a lock
  // on the mutex is safe.
  // We do not want to hold a lock on the mutex because doing so while calling
  // the PV providers’ fianlizeInitialization methods could result in a dead
  // lock if one of these methods acuqires a mutex that is also acquired before
  // calling one of this class’s methods.
  for (auto const &entry : PVProviderRegistry::pvProviders) {
    auto &pvProvider = entry.second;
    pvProvider->finalizeInitialization();
  }
}

PVProvider::SharedPtr PVProviderRegistry::getPVProvider(
    std::string const & name) {
  std::lock_guard<std::recursive_mutex> lock(PVProviderRegistry::mutex);
  try {
    return PVProviderRegistry::pvProviders.at(name);
  } catch (std::out_of_range &e) {
    throw std::invalid_argument(
      std::string("The name '") + name
        + "' does not reference a registered application or device.");
  }
}

void PVProviderRegistry::registerApplication(
      std::string const &appName,
      ControlSystemPVManager::SharedPtr pvManager) {
  std::lock_guard<std::recursive_mutex> lock(PVProviderRegistry::mutex);
  if (finalizeInitializationCalled) {
    throw std::logic_error(
        "Cannot register an application after "
        "PVProviderRegistry::finalizeInitialization has been called.");
  }
  if (PVProviderRegistry::pvProviders.find(appName)
      != PVProviderRegistry::pvProviders.end()) {
    throw std::invalid_argument(
      std::string("The name '") + appName + "' is already in use.");
  }
  auto pvProvider = std::make_shared<ControlSystemAdapterPVProvider>(pvManager);
  PVProviderRegistry::pvProviders.insert(std::make_pair(appName, pvProvider));
}

void PVProviderRegistry::registerDevice(
      std::string const &devName,
      std::string const &deviceNameAlias,
      std::size_t numberOfIoThreads) {
  std::lock_guard<std::recursive_mutex> lock(PVProviderRegistry::mutex);
  if (finalizeInitializationCalled) {
    throw std::logic_error(
        "Cannot register a device after "
        "PVProviderRegistry::finalizeInitialization has been called.");
  }
  if (PVProviderRegistry::pvProviders.find(devName)
      != PVProviderRegistry::pvProviders.end()) {
    throw std::invalid_argument(
      std::string("The name '") + devName + "' is already in use.");
  }
  auto pvProvider = std::make_shared<DeviceAccessPVProvider>(
    deviceNameAlias, numberOfIoThreads);
  PVProviderRegistry::pvProviders.insert(std::make_pair(devName, pvProvider));
}

// Static member variables need an instance...
bool PVProviderRegistry::finalizeInitializationCalled(false);
std::recursive_mutex PVProviderRegistry::mutex;
std::unordered_map<std::string, PVProvider::SharedPtr> PVProviderRegistry::pvProviders;


} // namespace EPICS
} // namespace ChimeraTK
