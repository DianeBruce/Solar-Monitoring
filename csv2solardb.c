/* Copyright (c) 2022,2023 Diane Bruce db@db.net
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
#include <unistd.h>
#include <sysexits.h>
#include <libpq-fe.h>
#include "snapshot.h"
#include "config_parser.h"
#include "solar_config.h"

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

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
			    {NULL,NULL}};

static void usage(const char *);
static void process_csv_file(FILE *fp, int force_update,
			     const char *dbhost, const char *dbport,
			     const char *dbname, const char *dbuser,
			     const char *dbpassword,
			     const char *dbtable);
static PGconn *connect_to_db(const char *dbhost, const char *dbport,
			     const char *dbname, const char *dbuser,
			     const char *dbpassword);
static int insertsql(PGconn *pg_conn, int force_update,
		     const char *dbtable, const char *date_time,
		     const char *data_list);
static int do_one_sql(PGconn *pg_conn, const char * sql);
static char input_buf[MAXBUF];

/*
 * This program simply takes lines of csv and stuffs them
 * into the solar db. This is useful if there is an outage and
 * the db has to be reconstructed from the remote data
 * 
 */
int
main(int argc, char* argv[])
{
	FILE *fp;
	int ch;
	char *progname;
	int force_update=0;
	int error = 0;
	
	/*
	 * Simple config parser is used to retrieve defaults
	 */
	(void)parse_config(SOLAR_GLOBAL_CONFIG, parse_table);
	/* If a .solar config exists, populate defaults */
	(void)parse_config(SOLAR_CONFIG, parse_table);

	progname = argv[0];
	/*
	 * defaults from config parser can be overridden here
	 */
	while ((ch = getopt(argc, argv, "fh:p:u:P:t:?")) != -1) {
		switch (ch) {
		case 'f':
			force_update = 1;
			break;
		case 'h':
			dbhost = strdup (optarg);
			break;
		case 'p':
			dbport = strdup (optarg);
			break;
		case 'u':
			dbuser = strdup (optarg);
			break;
		case 'P':
			dbpassword = strdup (optarg);
			break;
		case 't':
			dbtable = strdup (optarg);
			break;
		case '?':
		default:
			usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		csvfilename = strdup (*argv);
	else {
		error = EX_USAGE;
		fprintf (stderr, "No csvfilename given\n");
		csvfilename = NULL;
	}
	
	if (dbhost == NULL) {
		error = EX_USAGE;
		fprintf (stderr, "No db host in config file or -h host given\n");
	}
	if (dbport == NULL) {
		error = EX_USAGE;
		fprintf (stderr, "No db port in config file or -p port given\n");
	}
	if (dbuser == NULL) {
		error = EX_USAGE;
		fprintf (stderr, "No db user in config file or -u user given\n");
	}
	if (dbtable == NULL) {
		error = EX_USAGE;
		fprintf (stderr, "No db table in config file or -t table given\n");
	}
	if (dbpassword == NULL) {
		error = EX_USAGE;
		fprintf (stderr, "No db password in config file or -P password given\n");
	}

	fp = NULL;
	if (csvfilename != NULL ) {
		fp = fopen(csvfilename, "r");
		if ( NULL == fp ) {
			error = EX_USAGE;
			fprintf (stderr, "Can't open %s for reading\n",
				 csvfilename);
		}
	}

	if (error) {
		usage (progname);
		exit (error);
	}

	process_csv_file(fp, force_update, dbhost, dbport,
			 dbname, dbuser,
			 dbpassword,
			 dbtable);
	exit (EX_OK);
}

/*
 *
 * process_csv_file:	Takes lines of csv solar data and inserts
 *			them into solar db.
 *
 * inputs		- File fp pointing to csv file being read
 *			- force_update force update if duplicate key
 *			- name of data base host
 *			- port number to use
 *			- database name
 *			- database user
 *			- database password
 *			- database table name
 *
 * output		- None
 * side effects		- database is updated 
 */

static void
process_csv_file(FILE *fp, int force_update,
		 const char *dbhost, const char *dbport,
		 const char *dbname, const char *dbuser, const char *dbpassword,
		 const char *dbtable)
{
	char *p;
	char *input_p;
	char *date_time;
	PGconn *pg_conn;
	
	pg_conn = connect_to_db(dbhost, dbport, dbname, dbuser, dbpassword);
	if (pg_conn == NULL)
		err(EX_DATAERR, "Can't open database");

	while ((fgets(input_buf, MAXBUF-1, fp)) != NULL) {
		p = strchr(input_buf, '\n');	/* strip newline */
		if (p != NULL)
			*p = '\0';

		/*
		 * Additional code to remove diff -u strings
		 */
		if (strncmp(input_buf, "---", 3) == 0)
			continue;
		if (strncmp(input_buf, "+++", 3) == 0)
			continue;
		if (strncmp(input_buf, "@@", 2) == 0)
			continue;
		if (*input_buf == '-')
			continue;
		input_p = input_buf;
		if (*input_p == '+')
			input_p++;
		
		/*
		 * First data element in CSV line is time
		 * and is the primary key
		 * so it must be separated out. Simple to do here.
		 * Obviously if the csv file is malformed
		 * this could break.
		 */
		p = strchr(input_p, ',');
		if (p != NULL) {
			*p = '\0';
			date_time = input_p;
			p++;
			insertsql(pg_conn, force_update, dbtable, date_time, p);
		}
	}
	fclose (fp);
	PQfinish (pg_conn);
	exit (EX_OK);
}

/*
 * connect_to_db
 *
 * Simply connects to Postgres db and returns a PGconn *
 *
 * inputs	- dbhost hostname of database
 * 		- dbport port number of database
 *		- dbname database name
 *		- dbuser database user name
 *		- dbpassword database password
 * output	- return a struct PGconn * or NULL for error
 */
static PGconn *
connect_to_db(const char *dbhost, const char *dbport,
	      const char *dbname, const char *dbuser, const char *dbpassword)
{
	char *sql;
	PGconn *pg_conn;
	PGresult *pg_result;
	char *pg_error_msg;
	int status = 0;
	
	pg_conn = PQsetdbLogin(
		dbhost,
		dbport,
		NULL,
		NULL,
		dbname,
		dbuser,
		dbpassword
		);

	if (PQstatus(pg_conn) == CONNECTION_BAD) {
		pg_error_msg = PQerrorMessage(pg_conn);
		fprintf(stderr, "%s\n", pg_error_msg);
		PQfinish(pg_conn);
		return(NULL);
	}
	return (pg_conn);
}

/*
 * insertsql
 *
 * inputs:	- pg_conn struct *PGConn
 *		- force_update force update even if duplicate key
 *		- date_time date and time of snapshot
 *		- data_in rest of csv line
 * output:	- Return 1 if successful and return 0 if unsuccessful
 *
 */
static int
insertsql(PGconn *pg_conn, int force_update,
	  const char *dbtable, const char *date_time, const char *data_in)
{
	char *sql;
	char *delete_sql;
	int status = 1;

	asprintf(&sql,
		 "INSERT INTO %s(date_time,array_v,array_a,array_w,"
		 "soc,bat_v,bat_a,load_v,load_a) "
		 "values (timestamp'%s',%s);",dbtable, date_time, data_in);

	status = do_one_sql(pg_conn, sql);

	if (status == -2 && force_update) {
		asprintf(&delete_sql,
			 "DELETE FROM %s WHERE date_time = '%s';",
			 dbtable, date_time);
		status = do_one_sql(pg_conn, delete_sql);
		free(delete_sql);
		if (status > 0)
			status = do_one_sql(pg_conn, sql);
	}

	free(sql);
	return(status);
}

/*
 * Do one sql command
 *
 * inputs	- PGconn *pg_conn
 *		- sql to execute
 * output
 * 		Return status:
 * 		Success				 1
 * 		Failure due to duplicate key 	-2
 *		Other failure			-1
 */
static int
do_one_sql(PGconn *pg_conn, const char * sql)
{
	PGresult *pg_result;
	char *pg_error_msg;
	int status=1;
	
	pg_result = PQexec(pg_conn, sql);
				    
	if (pg_result != NULL &&
	    PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
		pg_error_msg = PQerrorMessage(pg_conn);
		if ((strstr(pg_error_msg , "duplicate key value")) != NULL)
			status = -2;
		else
			status = -1;
		PQclear(pg_result);
	}

	return (status);
}

/*
 * usage
 *
 * inputs: 	program name
 * output:	None
 * side effects:	print to user usage
 */
static void
usage(const char *progname)
{
	fprintf(stderr, "%s: Defaults can be set in ~/%s\n", progname, SOLAR_CONFIG);
	fprintf(stderr, "%s: Or in global %s\n", progname, SOLAR_GLOBAL_CONFIG);
	fprintf(stderr, "%s; Except for csvname which must be given on command line\n", progname);
	fprintf(stderr, "%s: -h database hostname\n", progname);
	fprintf(stderr, "%s: -p database portnumber\n", progname);
	fprintf(stderr, "%s: -u database username\n", progname);
	fprintf(stderr, "%s: -P database username password\n", progname);
	fprintf(stderr, "%s: -t database table name (usually 'solar')\n", progname);
	fprintf(stderr, "%s: csvname to read\n", progname);
}
