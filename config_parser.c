/* Copyright (c) 2022, 2023 Diane Bruce db@db.net
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

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <pwd.h>
#include "config_parser.h"

static char **find_found_token(char *found_token, PARSE_ITEMS parse_table);
static char input_buf[MAXCONFIGBUF];
static void parse_line(char *input_buf,PARSE_ITEMS parse_table);

/*
 * This subroutine parses file name in home directory of caller or
 * if the given file name has a path, then this is assumed to
 * be a system wide default config file.
 * Pretty simple config file with value=data lines. Not very resistant
 * to badly formed files. Sorry. # starting a line is a comment line.
 * 
 * basename is taken as the name of the config file in the home directory
 * You probably will be putting a '.' in front, I know I do.
 * The parse_table passed is a table of "variable names" and addresses
 * to stick the result into. If the variable name is found in the config
 * file everything after the = is stuffed into the address given.
 * Spaces are removed due to strsep use.
 */

/* parse_config
 *
 * inputs	- file name of conf file
 *		- a PARSE_ITEMS parse_table
 * output	- return -1 for not found conf file
 *		  1 for found conf file
 */

int
parse_config(const char *name, PARSE_ITEMS parse_table)
{
	int uid;
	struct passwd *pwd_uid;
	char *filename;
	FILE *fp;

	if (*name != '/') {
		uid = getuid();
		pwd_uid = getpwuid(uid);
		asprintf(&filename, "%s/%s", pwd_uid->pw_dir, name);
	} else
		filename = strdup(name);

	fp = fopen(filename, "r");
	if (fp == NULL)
		return (-1);

	/*
	 * comment is either '#' or '['
	 * Doing this means the [header] part from a python config file
	 * will be ignored.
	 */
	while (fgets(input_buf, sizeof(input_buf), fp) != NULL) {
		if (*input_buf != '#' && *input_buf != '[')
			parse_line(input_buf,parse_table);
	}
	
	free(filename);
	return (1);
}

/*
 * parse_line
 *
 * Take an input line and a PARSE_ITEMS parse_table
 * and fill in each variable given in parse_table with its
 * corresponding value.
 *
 */

static void
parse_line(char *input_buf, PARSE_ITEMS parse_table)
{
	char **ap, *argv[2];
	char *p;
	char *found_token;
	char *found_data;
	char **data_p;		/* pointer to where pointer to data is */
	
	p = strchr(input_buf, '\n');
	if (p != NULL)
		*p = '\0';

	/* I'm not proud. Pretty much man page for strsep */
	for (ap = argv; (*ap = strsep(&input_buf, " \t=")) != NULL;)
                   if (**ap != '\0')
                           if (++ap >= &argv[2])
                                   break;
	found_token=input_buf;
	
	data_p = find_found_token(argv[0], parse_table);

	if (data_p != NULL && argv[1] != NULL)
		*data_p = strdup(argv[1]);
}

/*
 * given pointer to token, I look in the parse table looking for match.
 * If I find a match, I return the address of the address string data
 * will be stored, otherwise a NULL.
 */
static char **
find_found_token(char *found_token, PARSE_ITEMS parse_table)
{
	int i;
	char **data_p;
	
	for (i = 0 ; ; i++) {
		if (parse_table[i].token == NULL)
			break;
		if (strcmp(found_token , parse_table[i].token) == 0){
			if (parse_table[i].data != NULL) {
				data_p = parse_table[i].data;
				return(data_p);	/* hidden goto! */
			} else
				return(NULL);
		}
	}

	return(NULL);
}
