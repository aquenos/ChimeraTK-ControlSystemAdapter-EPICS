ChimeraTK Control System Adapter for EPICS
==========================================

About
-----

The [ChimeraTK project](https://github.com/ChimeraTK/) provides an
[adapter](https://github.com/ChimeraTK/) that allows control-systems to
communicate with devices (typically MicroTCA / PCI Express devices) in a generic
way. This EPICS device support interfaces to that adapter, so that it makes it
very easy to integrate a hardware device (or rather a hardware device library)
that is based on ChimeraTK into EPICS.


Installation
------------

Please copy `configure/EXAMPLE_CONFIG_SITE.local` to
`configure/CONFIG_SITE.local` and `configure/EXAMPLE_RELEASE.local` to
`configure/RELEASE.local` and adjust the paths and compiler settings in these
files.

The ChimeraTK Control System Adapter for EPICS needs the Boost C++ libraries and
the ChimeraTK Control System Adapter core libraryr. If you compiled the
ChimeraTK Control System Adapter core, you most likely already have all required
Boost libraries. The ChimeraTK Control System Adapter core also needs the
(ChimeraTK Device Access library)[https://github.com/ChimeraTK/DeviceAccess/].
For historic reasons, this library is still called mtca4u-DeviceAccess.


Adding the device support to an EPICS IOC
-----------------------------------------

In order to use the device support and driver in an IOC, you have to add the
line

```
CHIMERATK_EPICS=/path/to/epics/modules/chimeratk
```

to the `configure/RELEASE` file of your IOC application. Assuming your IOC 
application name is "xxx", you also have to add the two lines

```
xxx_DBD += ChimeraTK-ControlSystemAdapter-EPICS.dbd
xxx_LIBS += ChimeraTK-ControlSystemAdapter-EPICS
```

to the file `xxxApp/src/Makefile`.

You can then use `DTYP` "ChimeraTK" for most record types. The device addresses
have the form "@deviceName processVariableName" where "deviceName" is the name
of the device that has been used when registering it with the `DeviceRegistry`
(see below) and "processVariableName" is the name of the process variable within
the device. Please refer to the documentation of the respective device or device
library to find out which process variables exist for a certain device.

For the waveform record, you have to use the DTYP "ChimeraTK input" or
"ChimeraTK output", depending on the direction the record should use.

The ChimeraTK Control System Adapter is designed in a way so that devices
typically notify the control-system when new data is available. For this reason,
you should typically set the `SCAN` field of input records to `I/O Intr` because
scanning is less efficient and might also result in events being lost.

You have to initialize the respective device or device library and then register
the device's `ControlSystemPVManager` with the
`ChimeraTK::EPICS::DeviceRegistry`.

For example:

```
ChimeraTK::EPICS::DeviceRegistry::registerDevice(
    "myDevice", controlSystemPVManager);
```

You can do this from the IOC's main function, but typically you will rather add
an IOC shell function for that purpose.


Contact
-------

You can report issues in this software though the issue system on the
[GitHub page for this project](https://github.com/aquenos/ChimeraTK-ControlSystemAdapter-EPICS/).

If you want to report security vulnerabilities, we kindly ask that you use the
contact information given on our web page (http://www.aquenos.com/) in order to
get in touch with us.

---

Copyright notice for the ChimeraTK Control System Adapter for EPICS:
 
Copyright 2017 aquenos GmbH

The ChimeraTK Control System Adapter for EPICS is free software: you can
redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

The ChimeraTK Control System Adapter for EPICS is distributed in the hope that
it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with 
the ChimeraTK Control System Adapter for EPICS. If not, see
<http://www.gnu.org/licenses/>.
 
---

This software uses EPICS Base. Copyright notice for EPICS Base:

Copyright (c) 1991-2007 UChicago Argonne LLC and The Regents of the University 
of California. All rights reserved.

For the license of EPICS base please refer to the LICENSE file in the EPICS base
directory.

---

This software uses various parts of the Boost library.

Copyright notice for Boost scoped_ptr:
(C) Copyright Greg Colvin and Beman Dawes 1998, 1999.
Copyright (c) 2001, 2002 Peter Dimov

Copyright notice for Boost shared_ptr:
(C) Copyright Greg Colvin and Beman Dawes 1998, 1999.
Copyright (c) 2001-2008 Peter Dimov

Copyright notice for Boost Thread:
Copyright (C) 2001-2003 William E. Kempf
(C) Copyright 2008-9 Anthony Williams

Copyright notifce for Boost Unordered:
Copyright (C) 2003-2004 Jeremy B. Maitin-Shepard.
Copyright (C) 2005-2008 Daniel James.

All components of the Boost library are licensed under the terms of the Boost
Software License, Version 1.0. See http://www.boost.org/LICENSE_1_0.txt for the
license text.

---

This software used the ChimeraTK Control System Adapter core. Copyright notice
for the ChimeraTK Control System Adapter core:

Copyright 2014-2017 Deutsches Elektronen-Synchrotron
Copyright 2014-2017 aquenos GmbH

---

This software used the ChimeraTK Device Access library. Copyright notice
for the ChimeraTK Device Access library:

Copyright Deutsches Elektronen-Synchrotron
