## You may have to change deviceAccessExample to something else
## everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/deviceAccessExample.dbd"
deviceAccessExample_registerRecordDeviceDriver pdbbase

## Set the path to the .dmap file. This is only needed when using aliases.
chimeraTKSetDMapFilePath("$(TOP)/example.dmap")

## Open the device
#
#  The first parameter is the name that we use in the EPICS records.
#  The second parameter is the device URI or the alias from the .dmap file.
#  In this example, we could have used sdm://./dummy=$(TOP)/example.map instead
#  of EXAMPLE_DEVICE.
#  The third parameter is the number of I/O threads. For the dummy device, only
#  having one I/O thread should be fine, but for devices where read/write
#  operations might actually take some time, it may be wise to use more than one
#  thread.
chimeraTKOpenAsyncDevice("exampleDev", "EXAMPLE_DEVICE", 1)

## Load record instances
#
#  The DEV macro is the name that we used in chimeraTKOpenAsyncDevice.
dbLoadRecords("db/exampleDev.db","P=Test:,R=DeviceAccess:,DEV=exampleDev")

cd "${TOP}/iocBoot/${IOC}"
iocInit
