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

FLAGS=`pkg-config --cflags --libs libdrm`
FLAGS+=-Wall -O0 -g

CFLAGS+=-DENABLE_CMEM_AREA_INIT

all:
	$(CC) -o capture capture.c $(FLAGS) $(CFLAGS) $(LDFLAGS) $(LIBS)

distclean: clean
clean:
	rm -f *.o
	rm -f capture
