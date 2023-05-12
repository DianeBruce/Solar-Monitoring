/* Copyright (c) 2019,2020,2021,2022,2023 Diane Bruce db@db.net
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This is code to handle modbus serial communication.
 *
 * There are three separate modbus protocols, this code only
 * handles Remote Terminal Unit (RTU).
 * The other two are an ASCII form of MODBUS on serial
 * and a TCP/IP form of MODBUS.
 * See "http://modbus.org/docs/PI_MBUS_300.pdf"
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libutil.h>
#include "modbus_crc.h"
#include "libmodbus.h"

#define MAX_PACKET	1024
#define MAXBUF		1024

struct modbus_dev {
	int station;
	int function;
	int addr;
	int cnt;
	unsigned short *data;
};

static struct modbus_dev *do_one_modbus_rx(unsigned short *data_buf);
static struct modbus_dev *receive_modbus_packet(int check, unsigned char, unsigned short *);
static struct modbus_dev *decode_modbus_packet(unsigned char *, int buflen, unsigned short *data);
static void send_to_modbus_dev(struct modbus_dev *modbus_dev, unsigned short data[]);

static int master_fd;
static int baud;

/*
 * do_one_modbus_rx
 * inputs	pointer to data_buf
 * output	pointer to decoded packet
 * side effects none
 *
 * Monitor serial port *to* modbus device, then decode one packet
 * received from modbus device. Return a pointer to a modbus_dev
 * struct if successful and NULL on failure.
 * The modbus_dev struct will contain the function code and data pointers.
 */

static struct modbus_dev *
do_one_modbus_rx(unsigned short *data_buf)
{
	fd_set	readfs;
	int	status;
	int	nread;
	struct timeval timeout;
	unsigned char readbuf[MAXBUF];
	int delay;
	
	FD_ZERO(&readfs);

	for(;;) {	
		FD_SET(master_fd, &readfs);
		/*
		 * XXX
		 * This timeout depends on baud rate and should
		 * be derived from that value! :-(
		 * modbus binary protocol uses a 3.5 char delay
		 * to signal the start and end of the packet.
		 * At 9.6 Kb that's roughly 1ms per bit so 10ms per byte
		 * (1 start 1 stop bit + 8 data bits) that's 336000 usec
		 */
		delay = (baud * 3.5) * 10;
		//		timeout.tv_usec = 350000;
		timeout.tv_usec = delay;
		timeout.tv_sec = 0;
		if ((status = select(FD_SETSIZE, &readfs, NULL, NULL, &timeout)) > 0){
			if (FD_ISSET(master_fd, &readfs)) {
				if (ioctl(master_fd, FIONREAD, &nread) != 0)
					return(NULL);
				if (nread > 0) {
					FD_CLR(master_fd, &readfs);
					if(read(master_fd, readbuf, 1) > 0)
						receive_modbus_packet(0, readbuf[0], data_buf);
					FD_CLR(master_fd, &readfs);
				}
			}
		} else if (status == 0)
			return(receive_modbus_packet(1, readbuf[0], data_buf));
	}
	return (NULL);
}

/*
 * Simply accumulate a modbus packet
 *
 * modbus binary protocol uses a 3.5 char delay to signal the start and
 * end of a packet. So simply accumulate a buffer until I either overflow
 * or check is set because the device stopped sending. If check is set
 * a complete packet should be available.
 */

static unsigned char buf[MAX_PACKET];

static struct modbus_dev *
receive_modbus_packet(int check, unsigned char c, unsigned short *data_buf)
{
	static unsigned char *bp=buf;
	static int buflen = 0;
	static struct modbus_dev *modbus_dev=NULL;
	
	/* Initialize or check for full buffer hence packet available
	 * If buffer decodes correctly, return a pointer to the modbus_dev 
	 */

	if (check) {
		if (buflen != 0)
			modbus_dev =
				decode_modbus_packet(buf, buflen, data_buf);
		else
			modbus_dev = NULL;
		buflen = 0;
		bp = buf;
		return(modbus_dev);
	} else {
		if (buflen < MAX_PACKET) {
			*bp++ = c;
			buflen++;
		}
	}
	return (NULL);
}

/*
 * Try to parse out the function, station, count, address, and byte count
 * from incoming packet and then verify the checksum.
 * If it's valid checksum return a pointer to a modbus_dev struct.
 */

static struct modbus_dev *
decode_modbus_packet(unsigned char * buf, int buflen, unsigned short *data_buf)
{
	unsigned short crc=0;
	unsigned short check_crc=0;
	unsigned short addr=0;
	int station=0;
	int function=0;
	int byte_count;
	int cnt=0;
	static struct modbus_dev modbus_decode;
	
	crc = buf[buflen - 1];
	crc += buf[buflen - 2] << 8;
	check_crc = crc16(buf, buflen - 2);

	if (crc == check_crc) {
		modbus_decode.station = buf[0] & 0xFF;
		modbus_decode.function = buf[1] & 0xFF;
		byte_count = buf[2];
		modbus_decode.cnt  = byte_count / 2;
		modbus_decode.data = data_buf;
		swab(buf+3, data_buf, byte_count);
		return (&modbus_decode); /* OK a valid modbus packet */
	} else
		return (NULL);		/* bad modbus packet */
}

/*
 * send_to_modbus_dev 
 * does the heavy lifting required to set up outgoing packet
 * then does the write
 */

static unsigned char sndbuf[MAXBUF];

static void
send_to_modbus_dev(struct modbus_dev *modbus_dev, unsigned short data[])
{
	int byte_count;
	int total=0;
	int crc_cnt=0;
	unsigned short send_crc;

	sndbuf[0] = modbus_dev->station & 0xFF;
	sndbuf[1] = modbus_dev->function & 0xFF;
	sndbuf[2] = (modbus_dev->addr >> 8) & 0xFF;
	sndbuf[3] = modbus_dev->addr & 0xFF;
	sndbuf[4] = (modbus_dev->cnt >> 8) & 0xFF;
	sndbuf[5] = modbus_dev->cnt & 0xFF;
	
	switch(modbus_dev->function) {
	case READ_HOLDING_REGISTERS:
		send_crc = crc16(sndbuf, 6);
		sndbuf[6] = (send_crc >> 8) & 0xFF;
		sndbuf[7] = send_crc & 0xFF;
		total = 8;
		break;
	case WRITE_MULTIPLE_REGISTERS:
		byte_count = 2 * modbus_dev->cnt;
		sndbuf[6] = byte_count;
		swab(data, sndbuf + 7, byte_count);
		/* 7 for header */
		crc_cnt = 7 + byte_count;
		total = crc_cnt + 2;
		send_crc = crc16(sndbuf, crc_cnt);
		sndbuf[crc_cnt] = (send_crc >> 8) & 0xFF;
		sndbuf[crc_cnt + 1] = send_crc & 0xFF;
		break;
	default:
		break;
	}
	
	write(master_fd, sndbuf, total);
}

/* Public facing functions */

/*
 * open_modbus is just given a tty name to open, returns -ve
 * if error.
 * Since the official spec uses the bit rate to adjust timing I will
 * need the baud rate to properly set that even if I don't adjut
 * the tty speed here.
 *
 * inputs	- tty_name the name of the tty to open
 * output	- tty fd or -1
 * side effects	- master_fd is set
 *
 * XXX should speed actually be set in this function?
 * or read from ioctl?
 */

static struct termios origtermsettings;

#define	RETRY_COUNT	5
int
open_modbus(const char *tty_name, speed_t speed)
{
	int status;
	int i;
	unsigned short *data;
	struct termios termsettings;
	int retry_count;

	receive_modbus_packet(1, 0, NULL);	/* Init only */
	baud = speed;
	master_fd = -1;
	/*
	 * It is possible that another process is reading the modbus
	 * hence try RETRY_COUNT times with short delay between each
	 * attempt. 1 second is plenty.
	 */
	retry_count = RETRY_COUNT;
	while (master_fd < 0 && retry_count > 0) {
		master_fd = open(tty_name, O_RDWR|O_EXLOCK|LOCK_NB);
		if (master_fd < 0) {
			if (errno != EAGAIN)
				return(-1);
			retry_count--;
			sleep(1);
		} else {
			tcgetattr(master_fd, &origtermsettings);
			tcgetattr(master_fd, &termsettings);
			cfmakeraw(&termsettings);
	
			termsettings.c_cflag = CS8|CREAD|CLOCAL;
			cfsetspeed(&termsettings, B9600);
			tcsetattr(master_fd, TCSANOW, &termsettings);
		}
	}

	return(master_fd);
}

int
close_modbus(int fd)
{
/* Ignore errors */
	
	tcsetattr(fd, TCSANOW, &origtermsettings);
	close(fd);
	return(-1);
}

static	unsigned short	temp_data[MAXBUF];

int
write_registers(int device_id, int count,
		unsigned short addr, unsigned short data[])
{
	static struct modbus_dev modbus_dev;
	static struct modbus_dev *modbus_response;

	modbus_dev.station = device_id;
	modbus_dev.function = WRITE_MULTIPLE_REGISTERS;
	modbus_dev.addr = addr;
	modbus_dev.cnt = count;
	send_to_modbus_dev(&modbus_dev, data);
	modbus_response = do_one_modbus_rx(temp_data);
	if (modbus_response == NULL)
		return (-1);
	else
		return (modbus_response->cnt);
}

int
read_registers(int device_id, int count,
		       unsigned short addr, unsigned short data[])
{
	static struct modbus_dev modbus_dev;
	static struct modbus_dev *modbus_response;

	modbus_dev.station = device_id;
	modbus_dev.function = READ_HOLDING_REGISTERS;
	modbus_dev.addr = addr;
	modbus_dev.cnt = count;
	send_to_modbus_dev(&modbus_dev, data);
	modbus_response = do_one_modbus_rx(data);
	if (modbus_response == NULL)
		return (-1);
	else
		return (modbus_response->cnt);
}
