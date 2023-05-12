/* Copyright (c) 2019,2020,2021,2022,2023
 * Diane Bruce, db@db.net
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

/* This program opens serial port set in either
 * ~/.solar or
 * ../etc/solar.conf
 *
 * It allows one to read MODBUS addresses using simple commands.
 * Useful for debugging a MODBUS device.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "renogy.h"
#include "libmodbus.h"
#include "config_parser.h"
#include "solar_config.h"

#define MAXBUF	1024
#define MAXCOUNT	40
#define MAX_ARGS	2
/*
 * Max memory available for history appears to be 0x800 (2048)
 * and each history block is 10 bytes
 */
#define MAX_REGISTERS	0x10000
#define HISTORY_OFFSET	0x10000
#define HISTORY_SIZE	10

short	access_data(int i);
int	day_offset(int day);

int	main(int argc, char *argv[]);
static void	main_loop(void);
static void	read_regs(int station, unsigned int addr, int count, int do_ascii);
static void	write_regs(int station, unsigned short addr, char *s);
static void	hex_dump (unsigned short data[], int count, int do_ascii);
static void	sig(int signo);
static void	help(void);

#define MODBUS_PORT_DEFAULT "/dev/cuaU0"
char *modport=MODBUS_PORT_DEFAULT;
PARSE_ITEMS parse_table = {{"modport", &modport},
			   {NULL, NULL}};

int
main(int argc, char *argv[])
{

	parse_config(SOLAR_GLOBAL_CONFIG, parse_table);
	parse_config(SOLAR_CONFIG, parse_table);

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, sig);
	if (signal(SIGTRAP, SIG_IGN) != SIG_IGN)
		signal(SIGTRAP, sig);
	if (signal(SIGFPE, SIG_IGN) != SIG_IGN)
		signal(SIGFPE, sig);
	if (signal(SIGBUS, SIG_IGN) != SIG_IGN)
		signal(SIGBUS, sig);
	if (signal(SIGSEGV, SIG_IGN) != SIG_IGN)
		signal(SIGSEGV, sig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sig);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, sig);

	main_loop();
	exit(EXIT_SUCCESS);
}

/*
 * main_loop
 *
 * Just sit here dealing with user commands.
 * Only commands recognised are:
 * read base count
 * readc base count	- attempts to dump in ASCII
 */

static void
main_loop(void)
{
	int status;
	int i,j;
	char *token;
	char *tofree;
	char *string;
	char *args[MAX_ARGS];
	char cmd_line[MAXBUF];
	unsigned int addr;
	unsigned short count;

	printf("> ");
	fflush(stdout);
	while (fgets(cmd_line, sizeof(cmd_line), stdin) != NULL) {
		if ((token = strchr(cmd_line, '\n')) != NULL)
			*token = '\0';
		string = tofree = strdup(cmd_line);
		i = 0;
		while ((token = strsep(&string, " ")) != NULL) {
			args[i++] = token;
			if (i >= MAX_ARGS)
				break;
		}
		if (i > 1 && args[1] != NULL)
			addr = strtoul(args[1], NULL, 0);
		else
			addr = 0x100;
		if (strcasecmp(args[0],"read") == 0) {
			if ( string != NULL)
				count = strtoul(string, NULL, 0);
			else
				count = 1;
			read_regs(1, addr, count, 0);
		} else if (strcasecmp(args[0],"readc") == 0) {
			if ( string != NULL)
				count = strtoul(string, NULL, 0);
			else
				count = 1;
			read_regs(1, addr, count, 1);
		} else if(strcasecmp(args[0], "write") == 0)
			write_regs(1, addr, string);
		else if(strcasecmp(args[0], "quit") == 0) {
			free(tofree);
			exit(EXIT_SUCCESS);
		}
		else
			help();

		free(tofree);
		printf("> ");
		fflush(stdout);
	}
}


/*
 * read_regs
 * inputs	station id (modbus station)
 *		address on this station
 *		count read this many bytes
 *		do dump in ascii
 * output	none (void)
 * side effects	print to stdout the data read from modbus read
 */

static void
read_regs(int station, unsigned int addr, int count, int do_ascii)
{
	int status;
	unsigned short data[MAXBUF];
	int day;
	int day_offset;
	int fd;

	fd = open_modbus(modport, B9600);

	if (fd < 0)
		err(EX_IOERR, "can't open modbus\n");

	if(addr >= 0xF000 && addr < 0x10000) {
		/* Ignore count since HISTORY_SIZE words are returned
		 * for each word read above 0xF000
		 */
		status = read_registers(station, count, addr, data);
		count = HISTORY_SIZE;
		day_offset = (addr & 0xFFF) * HISTORY_SIZE;
		hex_dump (data, count, do_ascii);
	} else {
		if (addr < 0xF000) {
			status = read_registers(station, count, addr, data);
			hex_dump (data, count, do_ascii);
		}
	}
	close(fd);
	printf("\n");
}

/*
 * write_regs
 * inputs	station id (modbus station)
 *		address on this station
 *		count write this many bytes
 * output	none (void)
 * side effects	writes data to the MPPT controller
 */

static void
write_regs(int station, unsigned short addr, char *s)
{
	int i;
	int count;
	int status;
	unsigned short data[MAXBUF];
	char *p;
	char *t;
	int fd;
	
	fd = open_modbus(modport, B9600);

	if (fd < 0)
		err(EX_IOERR, "can't open modbus\n");
	
	for (p = s,count = 0; count < MAXBUF; count++) {
		t = strsep(&p, " ");
		if (t == NULL)
			break;
		data[count] = strtoul(s, NULL, 0);
	}
	status = write_registers(station, count, addr, data);
	close(fd);
}

static void
hex_dump (unsigned short data[], int count, int do_ascii)
{
	int i;
	int c_lo;
	int c_hi;

	if (do_ascii) {
		for (i = 0; i < count; i++) {
			c_lo = data[i] & 0xFF;
			c_hi = (data[i] >> 8) & 0xFF;
			if (isascii(c_hi))
				printf("%c ", c_hi);
			else
				printf(". ");
			if (isascii(c_lo))
				printf("%c ", c_lo);
			else
				printf(". ");
		}
	} else {
		for (i = 0; i < count; i++) {
			printf("%04X ", data[i]);
		}
	}
}

static void
help(void)
{
	printf("read station address count\n");
	printf("\tdump as hex\n");
	printf("readc station address count\n");
	printf("\tdump as ASCII chars if possible\n");
	printf("write station count address data\n");
	printf("quit\n");
}

void
sig(int signo)
{
	switch(signo) {
	case SIGTERM:
		exit(EXIT_SUCCESS);
		break;
	case SIGALRM:
	case SIGBUS:
	case SIGFPE:
	case SIGHUP:
	case SIGTRAP:
	case SIGSEGV:
	default:
		signal(signo, SIG_DFL);
		kill(0, signo);
		/* NOTREACHED */
	}
}
