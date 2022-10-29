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

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include <ChimeraTK/SupportedUserTypes.h>

#include "ChimeraTK/EPICS/RecordAddress.h"

namespace ChimeraTK {
namespace EPICS {

namespace {

struct Options {
  bool noBidirectional = false;
};

class Parser {

public:

  Parser(std::string const &addressString)
      : addressString(addressString), position(0) {
  }

  RecordAddress parse() {
    auto foundAppOrDevName = appOrDevName();
    separator();
    auto foundPvName = pvName();
    Options foundOptions;
    bool expectValueType = false;
    // The value type might be omitted and the next element that we see might be
    // the options list, so we have to check which of the two it is.
    if (!isEndOfString()) {
      separator();
      // We can only peek if there is at least one character left, so we have to
      // check first.
      if (isEndOfString()) {
        throwException("Expected type specifier or \"(\", but found end of string.");
      }
      // We peek at the next character to decide wheter to expect a type specifier
      // or a list of options.
      if (peek() == '(') {
        foundOptions = options();
      } else {
        expectValueType = true;
      }
    }
    std::type_info const & foundValueType =
      expectValueType ? valueType() : typeid(void);
    // If there is a value type, the options list may follow. Otherwise, we
    // already handled the case where the is an options list, but no value type.
    if (!isEndOfString() && expectValueType) {
      separator();
      foundOptions = options();
    }
    if (!isEndOfString()) {
      throwException(std::string("Expected end of string, but found \"")
        + excerpt() + "\".");
    }
    return RecordAddress(foundAppOrDevName, foundPvName, foundValueType,
      expectValueType, foundOptions.noBidirectional);
  }

private:

  static std::string const appOrDevNameChars;
  static std::string const separatorChars;

  std::string addressString;
  std::size_t position;

  bool accept(std::string const &str) {
    if (addressString.length() - position < str.length()) {
      return false;
    }
    if (addressString.substr(position, str.length()) == str) {
      position += str.length();
      return true;
    } else {
      return false;
    }
  }

  bool acceptAnyOf(std::string const &characters) {
    if (isEndOfString()) {
      return false;
    }
    if (characters.find(peek()) != std::string::npos) {
      ++position;
      return true;
    } else {
      return false;
    }
  }

  bool acceptAnyNotOf(std::string const &characters) {
    if (isEndOfString()) {
      return false;
    }
    if (characters.find(peek()) == std::string::npos) {
      ++position;
      return true;
    } else {
      return false;
    }
  }

  std::string appOrDevName() {
    auto startPos = position;
    expectAnyOf(appOrDevNameChars);
    do {
    } while (acceptAnyOf(appOrDevNameChars));
    auto endPos = position;
    return addressString.substr(startPos, endPos - startPos);
  }

  std::string excerpt() {
    if (addressString.length() - position > 5) {
      return addressString.substr(position, 5);
    } else {
      return addressString.substr(position);
    }
  }

  void expect(std::string const &str) {
    if (!accept(str)) {
      if (isEndOfString()) {
        throwException(std::string("Expected \"") + str
          + "\", but found end of string.");
      } else {
        throwException(std::string("Expected \"") + str
          + "\", but found \"" + excerpt() + "\".");
      }
    }
  }

  void expectAnyOf(std::string const &characters) {
    if (!acceptAnyOf(characters)) {
      if (isEndOfString()) {
        throwException(std::string("Expected any of \"") + characters
          + "\", but found end of string.");
      } else {
        throwException(std::string("Expected any of \"") + characters
          + "\", but found \"" + peek() + "\".");
      }
    }
  }

  void expectAnyNotOf(std::string const &characters) {
    if (!acceptAnyNotOf(characters)) {
      if (isEndOfString()) {
        throwException(
          std::string("Expected any character that is not any of \"")
          + characters + "\", but found end of string.");
      } else {
        throwException(
          std::string("Expected any character that is not any of \"")
          + characters + "\", but found \"" + peek() + "\".");
      }
    }
  }

  bool isEndOfString() {
    return position == addressString.length();
  }

  Options options() {
    Options options;
    expect("(");
    if (accept("nobidirectional")) {
      options.noBidirectional = true;
    }
    expect(")");
    return options;
  }

  char peek() {
    return addressString.at(position);
  }

  std::string pvName() {
    auto startPos = position;
    expectAnyNotOf(separatorChars);
    do {
    } while (acceptAnyNotOf(separatorChars));
    auto endPos = position;
    return addressString.substr(startPos, endPos - startPos);
  }

  void separator() {
    expectAnyOf(separatorChars);
    do {
    } while (acceptAnyOf(separatorChars));
  }

  void throwException(std::string const &message) const {
    std::ostringstream os;
    os << "Error at character " << (position + 1)
      << " of the record address: " << message;
    throw std::invalid_argument(os.str());
  }

  std::type_info const & valueType() {
    if (isEndOfString()) {
      throwException("Expected type specifier, but found end of string.");
    } else if (accept("bool")) {
      return typeid(ChimeraTK::Boolean);
    } else if (accept("int8")) {
      return typeid(std::int8_t);
    } else if (accept("uint8")) {
      return typeid(std::uint8_t);
    } else if (accept("int16")) {
      return typeid(std::int16_t);
    } else if (accept("uint16")) {
      return typeid(std::uint16_t);
    } else if (accept("int32")) {
      return typeid(std::int32_t);
    } else if (accept("uint32")) {
      return typeid(std::uint32_t);
    } else if (accept("int64")) {
      return typeid(std::int64_t);
    } else if (accept("uint64")) {
      return typeid(std::uint64_t);
    } else if (accept("float")) {
      return typeid(float);
    } else if (accept("double")) {
      return typeid(double);
    } else if (accept("string")) {
      return typeid(std::string);
    } else if (accept("void")) {
      return typeid(ChimeraTK::Void);
    } else {
      throwException(std::string("Expected type specifier, but found \"")
        + excerpt() + "\".");
    }
    // This code is not reachable, but we need to throw an exception in order to
    // avoid getting a compiler warning.
    throw std::logic_error("This code should not have been reached.");
  }

};

std::string const Parser::appOrDevNameChars = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
std::string const Parser::separatorChars = std::string(" \t");

} // anonymous namespace

RecordAddress RecordAddress::parse(::DBLINK const &addressField) {
  if (addressField.type != INST_IO
      || addressField.value.instio.string == nullptr
      || addressField.value.instio.string[0] == 0) {
    throw std::invalid_argument(
      "Invalid device address. Maybe mixed up INP/OUT or forgot '@'?");
  }
  return Parser(addressField.value.instio.string).parse();
}

} // namespace EPICS
} // namespace ChimeraTK
