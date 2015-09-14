TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += mtca4uEpics

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install mtca4u.dbd into <top>/dbd
DBD += mtca4u.dbd

# specify all source files to be compiled and added to the library
mtca4uEpics_SRCS += mtca4uDeviceRegistry.cpp
mtca4uEpics_SRCS += mtca4uRecordDefinitions.cpp

mtca4uEpics_LIBS += boost_thread
mtca4uEpics_LIBS += ControlSystemAdapter

mtca4uEpics_LIBS += $(EPICS_BASE_IOC_LIBS)

boost_thread_DIR = $(BOOST_LIB)
ControlSystemAdapter_DIR = $(MTCA4U_LIB)

USR_INCLUDES += -I$(BOOST_INCLUDE)
USR_INCLUDES += -I$(MTCA4U_INCLUDE)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

