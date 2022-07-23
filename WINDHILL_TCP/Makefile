#Compiler
ARM_CC_53 = arm-linux-gnueabihf-gcc
ARM_CXX_53 = arm-linux-gnueabihf-g++
ARM_CC_83 = arm-linux-gnueabihf-8.3-gcc
ARM_CXX_83 = arm-linux-gnueabihf-8.3-g++

ifeq ($(GCC), gcc53)
	ARM_CC = ${ARM_CC_53}
	ARM_CXX = ${ARM_CXX_53}
else ifeq (${GCC}, gcc83)
	ARM_CC = ${ARM_CC_83}
	ARM_CXX = ${ARM_CXX_83}
else
#    $(error Usage: make GCC={gcc53|gcc83})
endif

#OBJECT
ifeq ($(GCC), gcc53)
	OBJECT = WINDHILL_TCP_c53
else ifeq (${GCC}, gcc83)
	OBJECT = WINDHILL_TCP_c83
endif

#DEFINITIONS
DEFS = -DTRACES_VERIFICATION

#Source Files
SOURCE_FILE = $(wildcard *.cpp)
SOURCE_FILE += $(wildcard ../Logger/*.cpp)
SOURCE_FILE += ../INI_Parse/ini.c
SOURCE_FILE += ../INI_Parse/INIReader.cpp
SOURCE_FILE += ../tfm_common.cpp

#INCLUDE
INCLS = -I.
INCLS += -I..
INCLS += -I../Logger

#LIBS to include
LIBS = -lpthread -mthumb -mthumb-interwork -D_REENTRANT
#STRIP
ARM_STRIP = arm-linux-gnueabihf-strip

#FLAGS
FLAGS = -Wall
FLAGS += -std=c++11
FLAGS += -Os

all:
	$(ARM_CXX) $(FLAGS) $(DEFS) -o$(OBJECT) $(SOURCE_FILE) $(INCLS) $(LIBS)
	$(ARM_STRIP) $(OBJECT)
