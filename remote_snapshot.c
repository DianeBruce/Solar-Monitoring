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

#include <err.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include "libmodbus.h"
#include "renogy.h"
#include "snapshot.h"
#include "config_parser.h"
#include "libsolar.h"
#include "solar_config.h"

char *modport=MODBUS_PORT_DEFAULT;
char *csvfilename=NULL;
char *ssh_host=NULL;
char *ssh_user=NULL;

PARSE_ITEMS parse_table = {{"modport", &modport},
			   {"csvfilename", &csvfilename},
			   {"ssh_host", &ssh_host},
			   {"ssh_user", &ssh_user},
			   {NULL,NULL}};

/*
 * Program to read from serial MODBUS connected to RENOGY MPPT charge controller
 * which decodes interesting data then forwards this data via ssh link to
 * a server which stores data in database and another copy of the csvfile.
 * Yes popen is not a super secure mechanism but this program will
 * be running on a dedicated not open to the world LAN computer (In my
 * case, a spare Raspberry pi)
 */
int
main(void)
{
	char *cmd;
	char *csv_line;
	FILE *fp;

	(void)parse_config(SOLAR_GLOBAL_CONFIG, parse_table);
	(void)parse_config(SOLAR_CONFIG, parse_table);

	if (csvfilename == NULL)
		err(EX_USAGE, "No csv filename in config file given\n");
	if (ssh_host == NULL || ssh_user == NULL)
		err(EX_USAGE, "No ssh user or host from config file given\n");

	csv_line = get_csv_snapshot(modport);	/* From libsolar */

	/* popen ssh isn't exactly secure and clever but it's
	 * on a small remote server. Who cares.
	 */

	asprintf(&cmd, "/usr/bin/ssh %s@%s", ssh_user, ssh_host);
	fp = popen(cmd,"w");

	/* send csv line to remote */
	fprintf(fp,"%s", csv_line);
	/* Add csv line to the local csv file */
	fp = fopen(csvfilename, "a");
	if (NULL != fp) {
		fprintf(fp,"%s", csv_line);
		fclose(fp);
	}
	free (csv_line);

	exit(EX_OK);
}

