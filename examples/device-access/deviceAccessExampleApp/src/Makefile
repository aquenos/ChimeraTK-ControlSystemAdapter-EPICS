TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = deviceAccessExample
# deviceAccessExample.dbd will be created and installed
DBD += deviceAccessExample.dbd

# deviceAccessExample.dbd will be made up from these files:
deviceAccessExample_DBD += base.dbd

# Include dbd files from all support applications:
deviceAccessExample_DBD += ChimeraTK-ControlSystemAdapter-EPICS.dbd

# Add all the support libraries needed by this IOC
deviceAccessExample_LIBS += ChimeraTK-ControlSystemAdapter-EPICS

# deviceAccessExample_registerRecordDeviceDriver.cpp derives from deviceAccessExample.dbd
deviceAccessExample_SRCS += deviceAccessExample_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
deviceAccessExample_SRCS_DEFAULT += deviceAccessExampleMain.cpp
deviceAccessExample_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#deviceAccessExample_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
deviceAccessExample_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

