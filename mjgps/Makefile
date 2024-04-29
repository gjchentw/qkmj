
# Makefile of QKMJ server

#for SunOS
#LINKS=-lresolv

#for SVR4  (Solaris)
#FLAGS=-DSVR4
#LINKS=-lsocket -lnsl -lresolv

#for FreeBSD
#LINKS=-lcrypt

CC=gcc

all: mjgps mjrec

mjgps: mjgps.c
	$(CC) mjgps.c $(FLAGS) $(LINKS) -o mjgps 

mjrec: mjrec.c
	$(CC) mjrec.c -o mjrec
