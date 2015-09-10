TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += mtca4u

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install mtca4u.dbd into <top>/dbd
DBD += mtca4u.dbd

# specify all source files to be compiled and added to the library
#mtca4u_SRCS += xxx

mtca4u_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

