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
	OBJECT = test_demo_c53
else ifeq (${GCC}, gcc83)
	OBJECT = test_demo_c83
endif

#DEFINITIONS
DEFS = -DTRACES_VERIFICATION
# 查看所有输出，发布时注释掉
#DEFS += -DTRACE_TRACES
# 调试信息，发布时注释掉
#DEFS += -DDEBUG_TRACES

#Source Files
SOURCE_FILE = $(wildcard ./DevConfig/*.cpp)
SOURCE_FILE += $(wildcard *.cpp)
SOURCE_FILE += $(wildcard ./tbox/*.cpp)
SOURCE_FILE += $(wildcard ./ConvertData/*.cpp)
SOURCE_FILE += $(wildcard ./PublishSignals/*.cpp)
SOURCE_FILE += $(wildcard ./Mosquitto/*.cpp)
SOURCE_FILE += $(wildcard ./CJson/*.c)
SOURCE_FILE += $(wildcard ./CCP/*.cpp)
SOURCE_FILE += $(wildcard ./XCP/*.cpp)
SOURCE_FILE += $(wildcard ./DBC/*.cpp)
SOURCE_FILE += $(wildcard ./OBD/*.cpp)
SOURCE_FILE += $(wildcard ./PowerManagement/*.cpp)
SOURCE_FILE += $(wildcard ./SaveFiles/*.cpp)
SOURCE_FILE += $(wildcard ./FTP/*.cpp)
SOURCE_FILE += $(wildcard ./Compress/*.cpp)
SOURCE_FILE += $(wildcard ./Logger/*.cpp)

#INCLUDE
INCLS = -I./DevConfig
INCLS += -I./tbox
INCLS += -I./ConvertData
INCLS += -I.
INCLS += -I./Logger
INCLS += -I./Third_Dyn_Lib/libssh2-1.9.0/include
# libftp.h
INCLS += -I./Third_Dyn_Lib/ftplib-4.0-1/src

#LIBS to include
ifeq ($(GCC), gcc53)
	LIBS = -L./Third_Dyn_Lib/libssh2-1.9.0/arm_gcc53_build/lib -lssh2
	LIBS += -L./Third_Dyn_Lib/mosquitto-1.6.12/arm_gcc53_build/usr/local/lib -lmosquitto
	LIBS += -L./Third_Dyn_Lib/ftplib-4.0-1/src/arm_gcc53_build/usr/lib -lftp
else ifeq (${GCC}, gcc83)
	# gcc 8.3 工具链中已含该库
	LIBS = -lssh2
	LIBS += -L./Third_Dyn_Lib/mosquitto-1.6.12/arm_gcc83_build/usr/local/lib -lmosquitto
	LIBS += -L./Third_Dyn_Lib/ftplib-4.0-1/src/arm_gcc83_build/usr/lib -lftp
endif

LIBS += -lz
LIBS += -ldl
LIBS += -lpthread -mthumb -mthumb-interwork -D_REENTRANT
LIBS += -lRTU_Module -lIOs_Module -lGPS2_Module -lGSM_Module -lINET_Module

#STRIP
ARM_STRIP = arm-linux-gnueabihf-strip

#FLAGS
FLAGS = -Wall
FLAGS += -std=c++11
FLAGS += -Os

.PHONY:
	all clean

all:
	$(ARM_CXX) $(FLAGS) $(DEFS) -o$(OBJECT) $(SOURCE_FILE) $(INCLS) $(LIBS)
	$(ARM_STRIP) $(OBJECT)
	
clean:
	-rm -rf *.o $(OBJECT)
