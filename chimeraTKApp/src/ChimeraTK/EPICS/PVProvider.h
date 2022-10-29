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

#ifndef CHIMERATK_EPICS_PV_PROVIDER_H
#define CHIMERATK_EPICS_PV_PROVIDER_H

#include <memory>
#include <stdexcept>
#include <typeinfo>

#include "PVSupport.h"

namespace ChimeraTK {
namespace EPICS {

/**
 * Interface for types that can provider PVSupport objects.
 * There are two implementations of this interface: The
 * ControlSystemAdapterPVProvider (which is backed by a ControlSystemPVManager
 * from the ChimeraTK Control System Adapter) and the DeviceAccessPVProvider
 * (which is backed by a Device from ChimeraTK Device Access).
 *
 * Instances of this class are safe for concurrent use by multiple threads, but
 * the PVSupport instances returned by createPVSupport(...) are not.
 */
class PVProvider {

public:

  /**
   * Type of a shared pointer to this type.
   */
  using SharedPtr = std::shared_ptr<PVProvider>;

  /**
   * Creates and returns the process-variable support object for the specified
   * process variable. Implementations may throw an exception if a
   * process-variable support object for the specified process variable has
   * already been created earlier.
   *
   * If there is no process variable with the specified name, this method throws
   * an exception.
   */
  template<typename ElementType>
  typename PVSupport<ElementType>::SharedPtr createPVSupport(
      std::string const &processVariableName);

  /**
   * Returns the default type for the specified process variable. The default
   * type is always compatible with the process variable. This means that it is
   * always possible to create a process-variable support using that type.
   *
   * In rare cases, it might not be possible to determine the default type. In
   * this case the return value is typeid(nullptr_t).
   *
   * If there is no process variable with the specified name, this method throws
   * an exception.
   */
  virtual std::type_info const &getDefaultType(
      std::string const &processVariableName) = 0;

protected:

  /**
   * Creates the process-variable support object for the specified process
   * variable. This method is called by
   * createPVSupport(std::string const &) and has to be implemented
   * by classes implementing this interface.
   *
   * This method returns a shared pointer to a process-variable support with the
   * specified element type. If it cannot return the pointer to such an object,
   * it throws an exception.
   */
  virtual PVSupportBase::SharedPtr createPVSupport(
      std::string const &processVariableName,
      std::type_info const &elementType) = 0;

  /**
   * Destructor. The destructor is protected so that instances cannot be
   * destroyed through a pointer to this interface.
   */
  virtual ~PVProvider() noexcept {
  }

};

template<typename ElementType>
typename PVSupport<ElementType>::SharedPtr PVProvider::createPVSupport(
    std::string const &processVariableName) {
  auto rawPtr = this->createPVSupport(processVariableName,
    typeid(ElementType));
  if (!rawPtr) {
    throw std::logic_error(
      "The createPVSupport(std::string const &, std::type_info const &) method returned a null pointer.");
  }
  auto typedPtr = std::dynamic_pointer_cast<PVSupport<ElementType>>(rawPtr);
  if (!typedPtr) {
    throw std::logic_error(
      "The createPVSupport(std::string const &, std::type_info const &) method returned a pointer that cannot be cast to PVSupport<ElementType>.");
  }
  return typedPtr;
}

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_PV_PROVIDER_H
