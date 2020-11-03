# -*- MakeFile -*-
# NAME: Copyright (c) 2020, Biren patel
# DESC: build unit tests for csv parser
# LISC: MIT License

#------------------------------------------------------------------------------#
# Compiler Setup
# Ignore Micrsoft fopen_s() warnings
#------------------------------------------------------------------------------#

cc = clang
cflag = -std=c99 -g -pedantic -Wall -Wextra -Wdouble-promotion -Wconversion \
		-Wnull-dereference -Wcast-qual -Wpacked -Wpadded \
		-D_CRT_SECURE_NO_DEPRECATE

#------------------------------------------------------------------------------#
# Objects
#------------------------------------------------------------------------------#

objects = csv_test.o csv.o unity.o

#------------------------------------------------------------------------------#
# Build
#------------------------------------------------------------------------------#

unit_test.exe : $(objects)
	$(cc) $(objects) -o unit_test.exe

csv_test.o : ../src/csv.h unity/unity.h csv_test.c
	$(cc) $(cflag) -c csv_test.c -I ../src -o csv_test.o

csv.o : ../src/csv.c ../src/csv.h
	$(cc) $(cflag) -c ../src/csv.c -o csv.o
	
unity.o : unity/unity.c unity/unity.h
	$(cc) -c unity/unity.c -o unity.o

#------------------------------------------------------------------------------#
# Post-Build
#------------------------------------------------------------------------------#

clean:
	del *.o
