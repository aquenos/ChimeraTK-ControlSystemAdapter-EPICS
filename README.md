ChimeraTK Control System Adapter for EPICS
==========================================

About
-----

The [ChimeraTK project](https://github.com/ChimeraTK/) provides an
[adapter](https://github.com/ChimeraTK/) that allows control-systems to
communicate with generic applications backed by hardware devices (typically
MicroTCA / PCI Express devices). This EPICS device support interfaces to that
adapter, in order to make it possible to use a ChimeraTK-based application in
EPICS.

When you are looking for a simple solution for running an application based on
[ChimeraTK's ApplicationCore](https://github.com/ChimeraTK/ApplicationCore),
you might be interested in the
[ChimeraTK ControlSystemAdapter EPICS-IOC Adapter](https://github.com/ChimeraTK/ControlSystemAdapter-EPICS-IOC-Adapter).
That project is based on this device support, but relieves you from most of the
work needed to create an EPICS IOC based on this device support.
This device support - on the other hand - is the right choice if you want to
create your own EPICS IOC in order to control all details.


Installation
------------

Please copy `configure/EXAMPLE_RELEASE.local` to `configure/RELEASE.local` and
adjust the path to EPICS Base.

Usually, the build system automatically detects the correct compiler and linker
flags. However, if the `ChimeraTK-ControlSystemAdapter-config` and
`mtca4u-deviceaccess-config` scripts are not in the `PATH`, you have to specify
the path to these scripts explicitly. In this case, please copy
`configure/EXAMPLE_CONFIG_SITE.local` to `configure/CONFIG_SITE.local` and
adjust the paths to these scripts.

The ChimeraTK Control System Adapter for EPICS needs the Boost C++ libraries,
the
[ChimeraTK Control System Adapter core library](https://github.com/ChimeraTK/ControlSystemAdapter/)
and the
[ChimeraTK Device Access library](https://github.com/ChimeraTK/DeviceAccess/).
If you compiled the ChimeraTK Control System Adapter core, you most likely
already have all required Boost libraries and the ChimeraTK Device Access
library. For historic reasons, the library provided by ChimeraTK Device Access
is still called `mtca4u-DeviceAccess`.


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
have the form "@applicationName processVariableName" where "applicationName" is
the name of the application that has been used when registering it with the
`DeviceRegistry` (see below) and "processVariableName" is the name of the
process variable within the application. Please refer to the documentation of
the respective application in order to find out which process variables exist
for that application.

For the waveform record, you have to use the DTYP "ChimeraTK input" or
"ChimeraTK output", depending on the direction the record should use.

The ChimeraTK Control System Adapter is designed in a way so that applications
typically notify the control-system when new data is available. For this reason,
you should typically set the `SCAN` field of input records to `I/O Intr` because
scanning is less efficient and might also result in events being lost.


Registering an application
--------------------------

There are two ways for registering an application. Most applications
automatically create an instance of `ApplicationBase`. In this case, it is
sufficient to link the IOC to the application library and add the following line
to `st.cmd`:

```
chimeraTKConfigureApplication("applicationName", 100)
```

In this example, `applicationName` is the same name that is also used in the `INP`
or `OUT` fields of records. This can be an arbitrary string, but it must not
contain whitespace because whitespace is used to separate the application name
from the process variable name in the input or output link. It is suggested to
only use alphanumeric characters and the underscore to allow for possible future
changes in the address format.

The polling rate (100 microseconds in this example) defines how often the IOC
checks whether value updates from the application are available. The default
value of 100 µs is a good compromise between low latency and CPU utilization
(checking once every 100 µs should not generate a significant overhead).

If an application is not registered automatically (e.g. because it does not
implement `ApplicationBase` so that more than one instance can be present in the
same server), it can still be used with this adapter.

In this case, you have to initialize the respective application or application
library and then register the application's `ControlSystemPVManager` with the
`ChimeraTK::EPICS::DeviceRegistry`.

For example:

```
ChimeraTK::EPICS::DeviceRegistry::registerDevice(
    "myApplication", controlSystemPVManager, 100);
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
redistribute it and/or modify it under the terms of the GNU Lesser General
Public License version 3 as published by the Free Software Foundation.

The ChimeraTK Control System Adapter for EPICS is distributed in the hope that
it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with the ChimeraTK Control System Adapter for EPICS. If not, see
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
