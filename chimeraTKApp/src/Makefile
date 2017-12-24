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
DBD += ChimeraTK-ControlSystemAdapter-EPICS.dbd

INC += ChimeraTK/EPICS/DeviceRegistry.h

# specify all source files to be compiled and added to the library
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += DeviceRegistry.cpp
ChimeraTK-ControlSystemAdapter-EPICS_SRCS += recordDefinitions.cpp

ChimeraTK-ControlSystemAdapter-EPICS_SYS_LIBS += boost_chrono$(BOOST_LIBNAME_SUFFIX)
ChimeraTK-ControlSystemAdapter-EPICS_SYS_LIBS += boost_thread$(BOOST_LIBNAME_SUFFIX)
ChimeraTK-ControlSystemAdapter-EPICS_SYS_LIBS += ChimeraTK-ControlSystemAdapter

ChimeraTK-ControlSystemAdapter-EPICS_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
