ChimeraTK Control System Adapter for EPICS
==========================================

About
-----

The [ChimeraTK project](https://github.com/ChimeraTK/) provides a set of
libraries that allow for the easy integration of hardware devices (in particular
devices based on MicroTCA / PCI Express) into control-system applications.

This EPICS device support provides an interface between EPICS and the ChimeraTK
core libraries. There are two ways in which this EPICS device support integrates
with ChimeraTK:

First, you can use this device support to integrate applications based on the
[ChimeraTK Control System Adapter](https://github.com/ChimeraTK/ControlSystemAdapter).
Typically, applications based on
[ChimeraTK Application Core](https://github.com/ChimeraTK/ApplicationCore) use
the Control System Adapter. If you have such an application you might be
interested in the
[ChimeraTK ControlSystemAdapter EPICS-IOC Adapter](https://github.com/ChimeraTK/ControlSystemAdapter-EPICS-IOC-Adapter).
That project is based on this device support, but relieves you from most of the
work needed to create an EPICS IOC based on this device support.
This device support - on the other hand - is the right choice if you want to
create your own EPICS IOC in order to control all details.

Second, you can use this device support to integrate devices that are supported
by [ChimeraTK Device Access](https://github.com/ChimeraTK/DeviceAccess). This is
useful when you have a device that is easily integrated with ChimeraTK Device
Access (e.g. because it is compatible with ChimeraTK's PCIe driver), but you do
not have an Application Core application yet and creating such an application
does not make sense because the device is only used for simple input and output
operations or you do not want to integrate it with other control-system
frameworks except EPICS.

Even though this device support tries to hide most of the inherent differences
between dealing with an application based on the ChimeraTK Control System
Adapter and a device based on ChimeraTK Device Access, a few subtle differences
remain. This documentation specifically points out these differences where
applicable.


Installation
------------

Please copy `configure/EXAMPLE_RELEASE.local` to `configure/RELEASE.local` and
adjust the path to EPICS Base.

Usually, the build system automatically detects the correct compiler and linker
flags. However, if the `ChimeraTK-ControlSystemAdapter-config` script is not in
the `PATH`, you have to specify the path to these scripts explicitly. In this
case, please copy `configure/EXAMPLE_CONFIG_SITE.local` to
`configure/CONFIG_SITE.local` and adjust the paths to these scripts.

The ChimeraTK Control System Adapter for EPICS needs the
[ChimeraTK Control System Adapter core library](https://github.com/ChimeraTK/ControlSystemAdapter/).

You need a compiler that supports C++ 14 in order to compile this device
support. This should not be a limitation because recent versions of ChimeraTK
Device Access and the Control System Adapter use C++ 14 features as well.


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

Registering an application
--------------------------

There are two ways for registering an application. Most applications
automatically create an instance of `ApplicationBase`. In this case, it is
sufficient to link the IOC to the application library and add the following line
to `st.cmd`:

```
chimeraTKConfigureApplication("applicationName")
```

In this example, `applicationName` is the same name that is also used in the `INP`
or `OUT` fields of records. This can be an arbitrary string, but it must only
contain ASCII letters, digits, and the underscore.

If an application is not registered automatically (e.g. because it does not
implement `ApplicationBase` so that more than one instance can be present in the
same server), it can still be used with this adapter.

In this case, you have to initialize the respective application or application
library and then register the application's `ControlSystemPVManager` with the
`ChimeraTK::EPICS::PVProviderRegistry`.

For example:

```
ChimeraTK::EPICS::PVProviderRegistry::registerApplication(
    "myApplication", controlSystemPVManager);
```

You can do this from the IOC's main function, but typically you will rather add
an IOC shell function for that purpose.


Registering a device
--------------------

A ChimeraTK Device Access device can be registered using the
`chimeraTKOpenAsyncDevice` and `chimeraTKOpenSyncDevice` IOC shell commands.

The `chimeraTKOpenAsyncDevice` command has the following syntax:

```
chimeraTKOpenAsyncDevice("myDev", "sdm://./dummy=/path/to/my.map", 1)
```

The first parameter is the device name that is used when referencing the device
in the `INP` or `OUT` field of EPICS records. This can be an arbitrary string,
but it must only contain ASCII letters, digits, and the underscore.

The second parameter is a URI or device name alias that is understood by
ChimeraTK Device Access. When using an alias, a `.dmap` file must be used.
Please see the section *Setting the path to the .dmap file* for details.

The third and last parameter is the number of I/O threads that are created.
There must be at least one I/O thread, but if the device is slow to respond
(e.g. because I/O operates over a network), increasing the number of I/O threads
may increase the throughput.

The `chimeraTKOpenSyncDevice` command has the following syntax:

```
chimeraTKOpenSyncDevice("myDev", "sdm://./dummy=/path/to/my.map")
```

The first and second parameter have the same meaning as for the
`chimeraTKOpenAsyncDevice` command. The third parameter does not exist because
the device will operate in synchronous mode. This means that no I/O thread is
created and all I/O operations are performed in the thread that processes the
record(s).

**Caution:** Never use the synchronous mode for a device where I/O operations
may block. This may cause the whole EPICS IOC to lock up. In particular,
synchronous mode should never be used with devices that are accessed over the
network. Even operating PCIe devices in this way may not be suitable because
there can be a significant delay when reading or writing data, in particular
if the size of the datasets is large. In case of doubt, use the asynchronous
mode because the overhead incurred by this mode should be negligible in most
cases.


Setting the path to the .dmap file
----------------------------------

When using device name aliases from a `.dmap` file, the path to that file must
be set. This can be done with the `chimeraTKSetDMapFilePath` IOC shell command.
That command has the following syntax:

```
chimeraTKSetDMapFilePath("/path/to/my.dmap")
```

This command must be used before `chimeraTKOpenAsyncDevice` and
`chimeraTKOpenSyncDevice` in order to be effective.


EPICS Records
-------------

This device support supports the `aai`, `aao`, `ai`, `ao`, `bi`, `bo`, `longin`,
`longout`, `mbbi`, `mbbo`, `mbbiDirect`, `mbboDirect`, `stringin`, and `stringout`
records. When using EPICS Base 3.16 or newer, the `int64in`, `int64out`, `lsi`
and `lso` records are supported as well.

The `DTYP` is `ChimeraTK` for all record types.

Please note that the numeric records only work with process variables of a
numeric type and the string records only work with process variables of type
`string`.

### Addresses

The addresses (in the record's `INP` or `OUT` field) have the form
`@applicationOrDeviceName processVariableName data-type (options)`.

The *applicationOrDeviceName* must be the name that has been specified when
calling the IOC shell command that registered it.

The *processVariableName* is the name of the process variable within the
application or of the register within the device. Please refer to the
documentation of the respective application or device in order to find out which
process variables or registers exist.

The *data-type* is optional and specifies the data type of the underlying
process variable or register. In most cases, the best data-type is detected
automatically. If explicitly specified, the data type must be `bool`, `int8`,
`uint8`, `int16`, `uint16`, `int32`, `uint32`, `int64`, `uint64`, `float`,
 `double`, `string`, or `void`.

The *options* are optional and are a list of comma-separated strings.

At the moment, the only supported option is `nobidirectional`. If set, this
option has the effect that output records will not be updated when the process
variable's value changes on the application or device side, even if such
bidirectional updates are supported for the process variable. For obvious
reasons, this option only has an effect for output records.

### Limitations

#### `aai` and `aao` record

When using an `aai` or `aao` record, the data type used by the record (and
specified in the record's `FTVL` field) must match the type of the underlying
process variable or register (except for signed / unsigned conversion).

As an exception to this rule, the `bool` data type is supported when combined
with an `FTVL` of `CHAR`, `SHORT`, or `LONG`.

When using a ChimeraTK Device Access, this can be achieved by explicitly
specifying the data type as part of the address. When using the ChimeraTK
Control System Adapter, the data type of the PV is fixed and thus the record's
data type must be set to match it.

The `aai` and `aao` records do not support the `int64`, `uint64`, and `void`
data-types.

#### `lsi` and `lso` record

The `lsi` and `lso` record are only supported when using EPICS Base 3.16 or
newer. They only work with process variables of type `string`.

#### `stringin` and `stringout` record

The `stringin` and `stringout` record only work with process variables of type
`string` and truncate long strings to 39 characters.

#### Numeric records

All numeric records (this means all records except `aai`, `aao`, `lsi`, `lso`,
`stringin`, and `stringout`) do not support process variables of type `string`.

#### ChimeraTK Control System Adapter

The ChimeraTK Control System Adapter is designed in a way so that applications
typically notify the control-system when new data is available. For this reason,
you should typically set the `SCAN` field of input records to `I/O Intr` because
scanning is less efficient and might also result in events being lost.

**Caution:** Do not use a periodic `SCAN` mode because this will almost
certainly not have the desired results. If the scan rate is higher than the
update rate of the underlying process variable, using periodic scanning will
simply result in an unnecessary overhead. When the scan rate is lower than the
update rate, you will miss updates.

Another limitation when using this device support with the ChimeraTK Control
System Adapter is that only one data type (the data type of the underlying
process variable) is supported. This means that there is no sense in explicitly
specifying a data type in an address referring to a PV of a ChimeraTK Control
System Adapter application.

#### ChimeraTK Device Access

This device support does currently not support setting `SCAN` to `I/O Intr` when
the record is connected to the register of a ChimeraTK Device Access device. One
of the reasons is that the support for asynchronous read operations in ChimeraTK
Device Access is still experimental. Another reason is that many device backends
(in particular the PCIe backend) do not implement asynchronous read operations
yet, so supporting them in the device support would be of limited use.

This device support only supports the data types listed in the section called
*Addresses*. In particular, it does not support registers that provide 64 bit
integers, unless these registers can also be accessed using one of the supported
data types. For non-numeric registers, the data type cannot be detected
automatically, even if they can be treated as numeric registers. In this case,
one of the supported data types has to be specified explicitly as part of the
register address specified in the record's `INP` or `OUT` field.

### Using Autosave

When using Autosave in order to persist the values of output records between
restarts of the IOC, please remember to set the respective records’ `PINI`
fields to `YES` (preferred), `RUN`, or `RUNNING`. Otherwise, the persited value
will be restored to the record, but the value will not be sent to the associated
application or device, so that it does not take effect.


Examples
--------

You can find an example IOC using a dummy ChimeraTK Device Access device in
`examples/device-access`. Please refer to the
[corresponding README file](examples/device-access/README.md) for details.


Contact
-------

You can report issues in this software though the issue system on the
[GitHub page for this project](https://github.com/aquenos/ChimeraTK-ControlSystemAdapter-EPICS/).

If you want to report security vulnerabilities, we kindly ask that you use the
contact information given on our web page (http://www.aquenos.com/) in order to
get in touch with us.


Copyright information
---------------------

Copyright notice for the ChimeraTK Control System Adapter for EPICS:

Copyright 2017-2022 aquenos GmbH

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
