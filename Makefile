#
# Makefile for Camera application test
#
# Copyright (C) 2016 Renesas Electronics Corporation
# Copyright (C) 2016 Cogent Embedded, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License.
#

CFLAGS  = -Wall -O0 -g
CXXFLAGS = -Wall -O0 -g
PKGCONFIG = pkg-config

DGB_FLAGS = -DENABLE_CMEM_AREA_INIT

DRM_CFLAGS  = $(shell $(PKGCONFIG) --cflags libdrm)
DRM_LIBS      = $(shell $(PKGCONFIG) --libs libdrm)

# Get OpenCV flags from pkg-config
OPENCV_CFLAGS  = $(shell $(PKGCONFIG) --cflags opencv4)
OPENCV_LIBS    = $(shell $(PKGCONFIG) --libs opencv4)

# Object files
OBJS = capture.o opencv_helper.o

all: capture

# Compile C file
capture.o: capture.c opencv_helper.h
	$(info +++++ capture.o)
	$(CC) $(CFLAGS) $(DRM_CFLAGS) $(DGB_FLAGS) -c capture.c -o capture.o

# Compile C++ file
opencv_helper.o: opencv_helper.cpp opencv_helper.h
	$(info +++++ opencv_helper.o)
	$(CXX) $(CXXFLAGS) $(OPENCV_CFLAGS) -c opencv_helper.cpp -o opencv_helper.o

# Link final binary with g++ (important!)
capture: $(OBJS)
	$(info +++++ capture)
	$(CXX) -o $@ $(OBJS) $(DRM_LIBS) $(OPENCV_LIBS)

clean:
	rm -f *.o
	rm -f capture
