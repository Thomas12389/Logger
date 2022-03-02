#! /bin/bash

g++ -o pc_test TCP_slave.cpp file_opt.cpp CTimer.cpp common.cpp ../Logger/Logger.cpp -I../ -lpthread
