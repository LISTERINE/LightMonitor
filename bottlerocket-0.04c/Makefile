# Generated automatically from Makefile.in by configure.
#
# Makefile for BottleRocket (controller for X10 FireCracker home automation
#  kit)
#

srcdir = .
top_srcdir = .
prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include

CFLAGS = -g -O2

CFLAGS += -I. -Wall  -O2 -DX10_PORTNAME=\"/dev/ttyS0\"
DEFS=-DHAVE_CONFIG_H
LIBS=
INSTALL= /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

CC=gcc

#
# uncomment if you want to use TX instead of DTR (necessary on some
#   Macs and with Boca boards, etc.
#

# DEFS += -DTIOCM_FOR_0=TIOCM_ST

br: br_cmd.o br.o
	${CC} ${CFLAGS} ${DEFS} -o br br.o br_cmd.o

br.o: ${srcdir}/br.c ${srcdir}/br_cmd.h
	${CC} ${CFLAGS} ${DEFS} -c ${srcdir}/br.c
	
br_cmd.o: ${srcdir}/br_cmd.c ${srcdir}/br_cmd.h ${srcdir}/br_translate.h
	${CC} ${CFLAGS} ${DEFS} -c ${srcdir}/br_cmd.c

install: br
	${INSTALL} -d -m 755 ${bindir}
	${INSTALL} -m 555 br ${bindir}

clean:
	-rm -f *.o br

really_clean: clean
	-rm -f config.h config.cache config.status config.log Makefile
