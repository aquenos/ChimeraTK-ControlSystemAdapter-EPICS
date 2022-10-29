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

#include <cstdint>
#include <stdexcept>
#include <string>

#include "ChimeraTK/EPICS/DeviceAccessPVSupport.h"

#include "ChimeraTK/EPICS/DeviceAccessPVProviderImpl.h"

namespace ChimeraTK {
namespace EPICS {

DeviceAccessPVProvider::DeviceAccessPVProvider(
    std::string const &deviceAliasName, int numberOfIoThreads)
    : ioExecutor(numberOfIoThreads) {
  if (numberOfIoThreads < 0) {
    throw std::invalid_argument(
      "The number of I/O threads must not be negative.");
  }
  this->insertCreatePVSupportFunc<std::int8_t>();
  this->insertCreatePVSupportFunc<std::uint8_t>();
  this->insertCreatePVSupportFunc<std::int16_t>();
  this->insertCreatePVSupportFunc<std::uint16_t>();
  this->insertCreatePVSupportFunc<std::int32_t>();
  this->insertCreatePVSupportFunc<std::uint32_t>();
  this->insertCreatePVSupportFunc<std::int64_t>();
  this->insertCreatePVSupportFunc<std::uint64_t>();
  this->insertCreatePVSupportFunc<float>();
  this->insertCreatePVSupportFunc<double>();
  this->insertCreatePVSupportFunc<std::string>();
  this->device.open(deviceAliasName);
  this->synchronous = numberOfIoThreads == 0;
}

DeviceAccessPVProvider::~DeviceAccessPVProvider() {
  this->device.close();
}

std::type_info const &DeviceAccessPVProvider::getDefaultType(
    std::string const &processVariableName) {
  auto registerCatalog = this->device.getRegisterCatalogue();
  RegisterPath registerPath(processVariableName);
  if (!registerCatalog.hasRegister(registerPath)) {
    throw std::invalid_argument(
      std::string("The process variable '") + processVariableName
      + "' does not exist.");
  }
  auto registerInfo = registerCatalog.getRegister(registerPath);
  auto &dataDescriptor = registerInfo.getDataDescriptor();
  switch (dataDescriptor.fundamentalType()) {
    case DataDescriptor::FundamentalType::numeric:
      if (dataDescriptor.isIntegral()) {
        if (dataDescriptor.isSigned()) {
          return typeid(std::int32_t);
        } else {
          return typeid(std::uint32_t);
        }
      } else {
        return typeid(double);
      }
    case DataDescriptor::FundamentalType::boolean:
      return typeid(std::uint32_t);
    default:
      return typeid(nullptr_t);
  }
}

bool DeviceAccessPVProvider::isSynchronous() {
  return this->synchronous;
}

std::shared_ptr<PVSupportBase> DeviceAccessPVProvider::createPVSupport(
    std::string const &processVariableName,
    std::type_info const &elementType) {
  try {
    auto createFunc = this->createPVSupportFuncs.at(
        std::type_index(elementType));
    return (this->*createFunc)(processVariableName);
  } catch (std::out_of_range &e) {
    throw std::runtime_error(
        std::string("The element type '") + elementType.name()
            + "' is not supported.");
  }
}

} // namespace EPICS
} // namespace ChimeraTK
