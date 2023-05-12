/* Copyright (c) 2019-2022 Diane Bruce
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
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include "snapshot.h"
#include "libmodbus.h"
#include "renogy.h"
#include "snapshot.h"
#include "config_parser.h"
#include "libsolar.h"
#include "solar_config.h"

char *dbhost=DEFAULT_DBHOST;
char *dbport=DEFAULT_DBPORT;
char *dbname=NULL;
char *dbuser=NULL;
char *dbpassword=NULL;
char *dbtable=NULL;
char *csvfilename=NULL;
char *modport=MODBUS_PORT_DEFAULT;

PARSE_ITEMS parse_table = {{"modport", &modport},
			   {"dbhost", &dbhost},
			   {"dbport", &dbport},
			   {"dbname", &dbname},
			   {"dbuser", &dbuser},
			   {"dbpassword", &dbpassword},
			   {"dbtable", &dbtable},
			   {"csvfilename", &csvfilename},
			    {NULL,NULL}};

/*
 * Program to read from serial MODBUS connected to RENOGY MPPT charge controller
 */
int
main(void)
{
	FILE *fp;
	char *csv_line;

	(void)parse_config(SOLAR_GLOBAL_CONFIG, parse_table);
	(void)parse_config(SOLAR_CONFIG, parse_table);

	if (dbhost == NULL)
		err(EX_USAGE, "No db host in config file given\n");
	if (dbport == NULL)
		err(EX_USAGE, "No db port in config file given\n");
	if (dbuser == NULL)
		err(EX_USAGE, "No db user in config file given\n");
	if (dbpassword == NULL)
		err(EX_USAGE, "No db password in config file given\n");
	if (dbtable == NULL)
		err(EX_USAGE, "No table name (dbtable) in config file given\n");
	if (csvfilename == NULL)
		err(EX_USAGE, "No csv filename in config file given\n");

	csv_line = get_csv_snapshot(modport);	/* From libsolar */

	/* Add csv line to the given csv file */
	fp = fopen(csvfilename, "a");
	if (NULL != fp) {
		fprintf(fp,"%s\n", csv_line);
		fclose(fp);
	}

	exit(EX_OK);
}

