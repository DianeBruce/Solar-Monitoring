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
#include <libpq-fe.h>
#include "config_parser.h"
#include "update_database.h"

/*
 * update_database:
 *
 * This function does the database update

 * Inputs: 
 * 	dbhost		- name of host to use
 *	dbport		- database port to use
 * 	dbname		- data base name
 *	dbuser		- data base username
 *	dbpassword	- password
 * 	csv_line	- one line of csv
 *
 * Output:
 * 			- Return 1 if successful and 0 if unsuccessful
 *
 */
int
update_database(const char *dbhost, const char *dbport,
		const char *dbname, const char *dbuser, const char *dbpassword,
		const char *dbtable,
		char *csv_line)
{
	char *p;
	char *date_time;
	char *data_in;
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
		return(0);
	}

	p = strchr(csv_line, ',');
	date_time = csv_line;
	if (p != NULL)
		*p = '\0';
	data_in = p + 1;
	
	asprintf(&sql,
		 "INSERT INTO %s(date_time,array_v,array_a,array_w,"
		"soc,bat_v,bat_a,load_v,load_a) "
		 "values (timestamp'%s',%s);",dbtable, date_time, data_in);

	pg_result = PQexec(pg_conn, sql);
	*p = ',';
	
	if (pg_result != NULL && PQresultStatus(pg_result) == PGRES_COMMAND_OK) {
		free(sql);
		pg_error_msg = PQerrorMessage(pg_conn);
		fprintf(stderr, "%s\n", pg_error_msg);
		PQclear(pg_result);
		PQfinish(pg_conn);
		return(0);

	}
	free(sql);
	PQfinish(pg_conn);

	return(1);
}
