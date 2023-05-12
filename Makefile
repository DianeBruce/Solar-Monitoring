# Copyright (c) 2022,2023
# Diane Bruce, db@db.net
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

PICFLAG=	-fPIC
PREFIX?=	/usr/local
LDFLAGS=	-L${PREFIX}/lib -L.
INCLUDE=	-I${PREFIX}/include -I.
CC?=	cc
CXX?=	c++

.SUFFIXES:	.pico .o

.c.pico:
	${CC} ${PICFLAG} -DPIC ${SHARED_CFLAGS} ${CFLAGS} ${INCLUDE} -c ${.IMPSRC} -o ${.TARGET}

.c.o:
	${CC} ${CFLAGS} ${INCLUDE} -o ${.TARGET} -c ${.IMPSRC}

all:
	@echo "make host OR remote side"
	@echo "make local if host and remote are on same machine"

web_status:	web_status.o libsolar.so config_parser.o
	${CC} -o web_status web_status.o config_parser.o -lsolar -lmodbus ${LDFLAGS}

web_status.o:	web_status.c web_status.h
	${CC} -o web_status.o -c web_status.c

local:	modbus local_snapshot csv2solardb web_status modbus_server

host:	recv_snapshot csv2solardb

remote:	modbus remote_snapshot web_status

modbus:	libmodbus.so modbus_server libsolar.so

csv2solardb:	csv2solardb.o config_parser.o
	${CC} -o csv2solardb csv2solardb.o config_parser.o -lpq ${LDFLAGS}

csv2solardb.o:	csv2solardb.c
	${CC} -c csv2solardb.c ${INCLUDE}

local_snapshot:	modbus local_snapshot.o config_parser.o update_database.o 
	${CC} ${CFLAGS} -o local_snapshot local_snapshot.o config_parser.o update_database.o -lmodbus -lsolar -lpq ${LDFLAGS} 

recv_snapshot:	recv_snapshot.o config_parser.o update_database.o snapshot.h
	${CC} ${CFLAGS} -o recv_snapshot recv_snapshot.o update_database.o config_parser.o -lpq ${LDFLAGS}

modbus_server:	modbus_server.o config_parser.o
	${CC}	-o modbus_server modbus_server.o config_parser.o -lmodbus ${LDFLAGS}

modbus_server.o:	modbus_server.c
	${CC} ${CFLAGS} -c -o modbus_server.o modbus_server.c

remote_snapshot: remote_snapshot.o libmodbus.so config_parser.o libsolar.so
	${CC} ${CFLAGS} -o remote_snapshot remote_snapshot.o config_parser.o \
	-lmodbus -lsolar ${LDFLAGS}

libmodbus.so:	libmodbus.pico modbus_crc.pico
	ld -shared -o libmodbus.so libmodbus.pico modbus_crc.pico

libsolar.so:	libsolar.pico libsolar.h
	ld -shared -o libsolar.so libsolar.pico

libsolar.pico:	libsolar.c libsolar.h
	${CC} ${PICFLAG} -DPIC ${SHARED_CFLAGS} ${CFLAGS} ${INCLUDE} -c ${.IMPSRC} -o ${.TARGET}

install_remote:
	install libmodbus.so ${PREFIX}/lib
	install libsolar.so ${PREFIX}/lib
	ldconfig ${INSTALLLIB}
	install modbus_server ${PREFIX}/bin
	install remote_snapshot ${PREFIX}/bin

install_host:
	install recv_snapshot ${PREFIX}/bin
	install csv2solarb ${PREFIX}/bin

install_both:
	mkdir ${INSTALLLIB}
	install libmodbus.so ${INSTALLLIB}
	install libsolar.so ${INSTALLLIB}
	install modbus_server ${PREFIX}/bin
	install local_snapshot ${PREFIX}/bin
	install csv2solarb ${PREFIX}/bin
	install web_status ${PREFIX}/bin

clean:
	rm -f recv_snapshot remote_snapshot local_snapshot modbus_server web_status csv2solardb *.pico *.so *.o

