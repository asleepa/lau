# change this variable to point to the directory with Lau headers
# of the version being tested
LAU_DIR = ../../

CC = gcc

# compilation should generate Dynamic-Link Libraries
CFLAGS = -Wall -O2 -I$(LAU_DIR) -fPIC -shared

# libraries used by the tests
all: lib1.so
	touch all

lib1.so: lib1.c $(LAU_DIR)/lauconf.h $(LAU_DIR)/lau.h
	$(CC) $(CFLAGS) -o lib1.so lib1.c