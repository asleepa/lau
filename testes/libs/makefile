# change this variable to point to the directory with Lau headers
# of the version being tested
LAU_DIR = ../../

CC = gcc

# compilation should generate Dynamic-Link Libraries
CFLAGS = -Wall -O2 -I$(LAU_DIR) -fPIC -shared

# libraries used by the tests
all: lib1.so lib11.so lib2.so lib21.so lib2-v2.so
	touch all

lib1.so: lib1.c $(LAU_DIR)/lauconf.h $(LAU_DIR)/lau.h
	$(CC) $(CFLAGS) -o lib1.so lib1.c

lib11.so: lib11.c $(LAU_DIR)/lauconf.h $(LAU_DIR)/lau.h
	$(CC) $(CFLAGS) -o lib11.so lib11.c

lib2.so: lib2.c $(LAU_DIR)/lauconf.h $(LAU_DIR)/lau.h
	$(CC) $(CFLAGS) -o lib2.so lib2.c

lib21.so: lib21.c $(LAU_DIR)/lauconf.h $(LAU_DIR)/lau.h
	$(CC) $(CFLAGS) -o lib21.so lib21.c

lib2-v2.so: lib21.c $(LAU_DIR)/lauconf.h $(LAU_DIR)/lau.h
	$(CC) $(CFLAGS) -o lib2-v2.so lib22.c
