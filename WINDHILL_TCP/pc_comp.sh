#! /bin/bash

g++ -o pc_test TCP_slave.cpp file_opt.cpp time_opt.cpp get_status.cpp get_version.cpp CTimer.cpp ../tfm_common.cpp ../Logger/Logger.cpp ../INI_Parse/INIReader.cpp ../INI_Parse/ini.c -I../ -lpthread -DPC_TEST
