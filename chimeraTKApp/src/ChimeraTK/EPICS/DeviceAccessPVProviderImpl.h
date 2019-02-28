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

#ifndef CHIMERATK_EPICS_DEVICE_ACCESS_PV_PROVIDER_IMPL_H
#define CHIMERATK_EPICS_DEVICE_ACCESS_PV_PROVIDER_IMPL_H

#include "DeviceAccessPVProviderDef.h"
#include "DeviceAccessPVSupport.h"

namespace ChimeraTK {
namespace EPICS {

template<typename T>
PVSupportBase::SharedPtr DeviceAccessPVProvider::createPVSupportInternal(
    std::string const &processVariableName) {
  return std::make_shared<DeviceAccessPVSupport<T>>(
    this->shared_from_this(), processVariableName);
}

template<typename T>
void DeviceAccessPVProvider::insertCreatePVSupportFunc() {
  this->createPVSupportFuncs.insert(
      std::make_pair(
          std::type_index(typeid(T)),
          &DeviceAccessPVProvider::createPVSupportInternal<T>));
}

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

#endif // CHIMERATK_EPICS_DEVICE_ACCESS_PV_PROVIDER_IMPL_H
