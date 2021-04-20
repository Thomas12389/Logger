#Compiler
ARM_CC = arm-linux-gnueabihf-gcc
ARM_CXX = arm-linux-gnueabihf-g++
#OBJECT
OBJECT = test_demo
#DEFINITIONS
DEFS = -DTRACES_VERIFICATION
DEFS += -DTRACE_TRACES	# 查看所有输出，发布时注释掉
DEFS += -DDEBUG_TRACES	# 调试信息，发布时注释掉

#Source Files
SOURCE_FILE = $(wildcard ./DevConfig/*.cpp)
SOURCE_FILE += $(wildcard ./tbox/*.cpp)
SOURCE_FILE += $(wildcard ./ConvertData/*.cpp)
SOURCE_FILE += $(wildcard ./PublishSignals/*.cpp)
SOURCE_FILE += $(wildcard ./Mosquitto/*.cpp)
SOURCE_FILE += $(wildcard ./CJson/*.c)
SOURCE_FILE += $(wildcard ./CCP/*.cpp)
SOURCE_FILE += $(wildcard ./DBC/*.cpp)
SOURCE_FILE += $(wildcard ./OBD/*.cpp)
SOURCE_FILE += $(wildcard ./PowerManagement/*.cpp)
SOURCE_FILE += $(wildcard ./SaveFiles/*.cpp)
SOURCE_FILE += $(wildcard ./FTP/*.cpp)
SOURCE_FILE += $(wildcard ./Compress/*.cpp)
SOURCE_FILE += $(wildcard *.cpp)
SOURCE_FILE += $(wildcard ./Logger/*.cpp)

#INCLUDE
INCLS = -I./DevConfig
INCLS += -I./tbox
INCLS += -I./ConvertData
INCLS += -I.
INCLS += -I./Logger
INCLS += -I/home/windhill/Desktop/libssh2/include

#LIBS to include
LIBS = -L/home/windhill/libssh2-1.9.0/arm_build/lib -lssh2
LIBS += -L/home/windhill/mosquitto-1.6.12/lib -lmosquitto
LIBS += -lz
LIBS += -ldl
LIBS += -lpthread -mthumb -mthumb-interwork -D_REENTRANT
LIBS += -lRTU_Module -lIOs_Module -lGPS2_Module -lGSM_Module -lINET_Module
#STRIP
ARM_STRIP = arm-linux-gnueabihf-strip

all:
	$(ARM_CXX) -Wall -std=c++11 $(DEFS) -o$(OBJECT) $(SOURCE_FILE) $(INCLS) $(LIBS)
	$(ARM_STRIP) $(OBJECT)
