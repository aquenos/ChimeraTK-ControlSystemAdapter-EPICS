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

#ifndef CHIMERATK_EPICS_ERROR_H
#define CHIMERATK_EPICS_ERROR_H

namespace ChimeraTK {
namespace EPICS {

/**
 * Prints an error message. Only the specified message (without any extra
 * information) is printed to stderr. A newline character is automatically
 * appended to the message.
 */
void errorPrintf(const char *format, ...) noexcept;

/**
 * Prints an error message with the current time and the name of the current
 * thread to stderr. A newline character is automatically appended to the
 * message.
 */
void errorExtendedPrintf(const char *format, ...) noexcept;

} // namespace EPICS
} // namespace ChimeraTK

#endif // CHIMERATK_EPICS_ERROR_H
