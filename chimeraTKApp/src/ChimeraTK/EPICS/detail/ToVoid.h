/*
 * ChimeraTK control-system adapter for EPICS.
 *
 * Copyright 2015-2019 aquenos GmbH
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

#ifndef CHIMERATK_EPICS_DETAIL_TO_VOID_H
#define CHIMERATK_EPICS_DETAIL_TO_VOID_H

#include <type_traits>

/**
 * Helper template struct for the ToVoid template using declaration.
 */
template<typename>
struct ToVoidHelper {
  using type = void;
};

/**
 * Resolves to the void type for all template parameters. The trick is that this
 * allows us to use decltype in a template parameter and know that this template
 * parameter is going to be void exactly if the decltype expression is actually
 * valid, regardless of which type it produces.
 */
template<typename T>
using ToVoid = typename ToVoidHelper<T>::type;

namespace ChimeraTK {
namespace EPICS {

namespace detail {

} // namespace detail
} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_DETAIL_TO_VOID_H
