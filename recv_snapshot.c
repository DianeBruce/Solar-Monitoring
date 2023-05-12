/* Copyright (c) 2022,2023
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "snapshot.h"
#include "config_parser.h"
#include "update_database.h"

#define MAXBUF 1024

char *dbhost=DEFAULT_DBHOST;
char *dbport=DEFAULT_DBPORT;
char *dbname=NULL;
char *dbuser=NULL;
char *dbpassword=NULL;
char *dbtable=NULL;
char *csvfilename=NULL;

PARSE_ITEMS parse_table = {{"dbhost", &dbhost},
			   {"dbport", &dbport},
			   {"dbname", &dbname},
			   {"dbuser", &dbuser},
			   {"dbpassword", &dbpassword},
			   {"dbtable", &dbtable},
			   {"csvfilename", &csvfilename},
			    {NULL,NULL}};

static char input_buf[MAXBUF];

/*
 * This program does nothing but gets forced envoked using sshd config
 * to receive a csvfile on standard input.
 *
 * e.g. On host in etc/ssh/sshd_config
 * Match User solar
 *       X11Forwarding no
 *       AllowTcpForwarding no
 *       PermitTTY yes
 *       ForceCommand /usr/local/bin/recv_snapshot
 *
 *
 * When run it then makes a csvfile copy of input csv line
 * adding to file name given in ~/.solar config file and
 * then enters this data into a postgres database using dbhost, dbport,
 * dbname, dbuser, dbpassword from the ~/.solar config file.
 * The table it updates is given by 'dbtable' 
 *
 * BUGS: No sanity checcking on csv input line.
 */
int
main(int argc, char* argv[])
{
	FILE *fp;
	char *p;
	
	if (parse_config(".solar", parse_table) < 0)
		err(-1, "Can't find config file");
	if (dbhost == NULL)
		err(-1, "No db host in config file given\n");
	if (dbport == NULL)
		err(-1, "No db port in config file given\n");
	if (dbuser == NULL)
		err(-1, "No db user in config file given\n");
	if (dbpassword == NULL)
		err(-1, "No db password in config file given\n");
	if (csvfilename == NULL)
		err(-1, "No csv filename in config file given\n");
	if (dbtable == NULL)
		err(-1, "No table name (dbtable) in config file given\n");

	fgets(input_buf, MAXBUF-1, stdin);
	p = strchr(input_buf, '\n');		/* strip newline */
	if (p != NULL)
		*p = '\0';
	
	if (csvfilename != NULL) {
		fp = fopen(csvfilename, "a");
		if (fp != NULL) {
			fprintf(fp, "%s\n",input_buf);
			fclose(fp);
		}
	}
	update_database(dbhost, dbport, dbname, dbuser, dbpassword,
			dbtable, input_buf);
}

