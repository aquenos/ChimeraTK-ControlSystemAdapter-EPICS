TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += ChimeraTK-ControlSystemAdapter-EPICS

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install chimeraTK.dbd into <top>/dbd
DBDCAT += ChimeraTK-ControlSystemAdapter-EPICS.dbd

ChimeraTK-ControlSystemAdapter-EPICS_DBD += common.dbd
ifdef BASE_3_16
ChimeraTK-ControlSystemAdapter-EPICS_DBD += int64.dbd
ChimeraTK-ControlSystemAdapter-EPICS_DBD += longString.dbd
endif

# specify all source files to be compiled and added to the library
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += ControlSystemAdapterPVProvider.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += DeviceAccessPVProvider.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += PVProviderRegistry.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += ThreadPoolExecutor.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += Timer.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += RecordAddress.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += errorPrint.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += recordDefinitions.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += registrar.cpp

ChimeraTK-ControlSystemAdapter-EPICS_LIBS += $(EPICS_BASE_IOC_LIBS)

ChimeraTK-ControlSystemAdapter-EPICS_SYS_LIBS += ChimeraTK-ControlSystemAdapter

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
