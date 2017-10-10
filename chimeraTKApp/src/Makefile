TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += chimeraTkEpics

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install mtca4u.dbd into <top>/dbd
DBD += chimeraTK.dbd

INC += ChimeraTKDeviceRegistry.h

# specify all source files to be compiled and added to the library
chimeraTkEpics_SRCS += ChimeraTKDeviceRegistry.cpp
chimeraTkEpics_SRCS += ChimeraTKRecordDefinitions.cpp

chimeraTkEpics_SYS_LIBS += boost_thread-mt
chimeraTkEpics_SYS_LIBS += ChimeraTK-ControlSystemAdapter

chimeraTkEpics_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
