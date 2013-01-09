# Makefile is used to build the application
# See README for more details
# Dept. of Computer Science, University at Buffalo
# Project - 3
# Author: Sanket Kulkarni
# 2012

CFLAGS = -I
CC = gcc

decserver: decserver.c
	$(CC) -o decserver decserver.c

decclient: decclient.c
	$(CC) -o decclient decclient.c
