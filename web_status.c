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

/*
 * web_status
 *
 * program runs on remote data collection (Raspberry Pi in my case
 * as of September 19, 2022- db) and presents a highly simplified
 * web server. No checking of proper GET / from client is done a simple
 * check for a /history argument results in a history listing
 * otherwise a simple status of current state of charging system is done.
 */
#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sysexits.h>
#include "config_parser.h"
#include "libsolar.h"
#include "solar_config.h"

char *modport;

PARSE_ITEMS parse_table = {
			   {"modport", &modport},
			    {NULL,NULL}};

static void do_http(FILE *fp);
static void web_status(FILE *fp);
static void web_history_status(FILE *fp, int day1, int day2);
static void webprintf(FILE *fp, char *hdr, char *fmt, ...);
static char *striptz(char *digits);
static void page_header(FILE *fp);

#define MAXLINE 100
#define BACKLOG 4

/*
 * web_status produces a simple http response detailing the solar
 * charging status from a Renogy controller.
 */

int
main(int argc, char* argv[])
{
	int s;
	int status;
	struct sockaddr_in sa;
	int connfd;
	FILE *fp;
	socklen_t b;
	struct passwd *pw;
	struct group *grp;
	gid_t gidset[3];
	int opt;
	socklen_t optlen=sizeof(opt);
	
	if (parse_config(SOLAR_GLOBAL_CONFIG, parse_table) < 0)
		err(EX_DATAERR, "Can't find config file");

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		err(EX_OSERR, "Socket error");
	if ((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) < 0)
		err(EX_OSERR, "setsockopt error");
	if ((setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))< 0)
		err(EX_OSERR, "setsockopt error");

	bzero(&sa, sizeof(sa));

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = 0;
	sa.sin_port = htons(80);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((status = bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0))
		err(EX_OSERR, "bind error");
	
	pw = getpwnam(SOLAR_USER);
	if (pw == NULL)
		err(EX_NOUSER, "%s does not exist", SOLAR_USER);

/* dialer group has access to serial ports */

	grp = getgrnam("dialer");
	if (grp == NULL)
		err(EX_NOUSER, "%s does not exist", "dialer");
	
	gidset[0] = pw->pw_gid;
	gidset[1] = pw->pw_gid;
	gidset[2] = grp->gr_gid;

	if (setgroups(3, gidset) < 0)
		err(EX_OSERR, "Can't set groups");
	setgid(pw->pw_gid);
	setuid(pw->pw_uid);

	switch (fork()) {
	case -1:
		err(EX_OSERR, "fork");
		break;
	default:
		close (s);
		return (0);
		break;
	case 0:
		break;
	}

	listen(s, BACKLOG);

	for(;;){
	        if ((connfd = accept(s, (struct sockaddr *)&sa, &b)) < 0)
			continue;

		if ((fp = fdopen(connfd, "r+")) == NULL)
			err(EX_OSERR, "fdopen");
		setvbuf(fp, NULL, _IOLBF, 0);
		fprintf(fp, "HTTP/1.1 200 OK\n\n");
		do_http(fp);
		fclose(fp);
		close(connfd);
	}
}

/*
 * read one line from client. Don't bother checking for proper GETS etc.
 * simply invoke history or normal status depending on /history
 * found in the line. This will handle http://solar.local?history etc.
 */
static void
do_http(FILE *fp)
{
	char *p;
	char *date_time;
	char line[MAXLINE];
	char **ap, *args[2];
	int day1=0;
	int day2=10;

	fgets(line, sizeof(line), fp);
	p = strchr(line, '/');
	if (p != NULL) {
		p++;
		if (strncmp(p, "history",7) == 0) {
			p += 7;
			if (*p == '?') {
				p++;

				for (ap=args;(*ap = strsep(&p, " ,")) != NULL;)
					if (**ap != '\0')
						if (++ap >= &args[2])
							break;
				if (args[0] != NULL)
					day1 = atoi(args[0]);
				if (args[1] != NULL)
					day2 = atoi(args[1]);
				if (day2 < day1)
					day2 = day1;
				web_history_status(fp, day1, day2);
			}
		} else
			web_status(fp);
	} else
		web_status(fp);
}

/*
 * web_status
 *
 * input	- fp File pointer to opened remote browser
 * output	- none
 * side effects	- solar status for remote browser in html format
 */
static void
web_status(FILE *fp)
{
	SOLAR_INFO *sol_info;

	sol_info = get_solar_info(modport);

	page_header(fp);
	fprintf(fp, "<div class=\"header\">\n");
	webprintf(fp, "h1","Solar Panel Status");
	webprintf(fp, "h2","Product Model: %s<br>"
		  "Hardware Version: %s<br>"
		  "Software Version: %s<br>"
		  "Serial number: %s</h2>",
		  sol_info->model,
		  sol_info->hardware_version,
		  sol_info->software_version,
		  sol_info->serial_number);
	fprintf(fp, "</div>\n");
	       
	webprintf(fp, "h2","Array Information");
	fprintf(fp, "<div class =\"grid-container\">\n");
	webprintf(fp, "div","Array Voltage: %.3fV", sol_info->array_v);
	webprintf(fp, "div","Array Current: %.3fA", sol_info->array_a);
	webprintf(fp, "div","Array Power: %dW", sol_info->array_w);
	webprintf(fp, "div","Array working state: %s",
		  sol_info->array_working_state);
	webprintf(fp, "div","Power Generated Today: %dW",
		  sol_info->power_gen_today);
	fprintf(fp, "</div>\n");
		  
	webprintf(fp, "h2","Battery Information");
	fprintf(fp, "<div class =\"grid-container\">\n");
	webprintf(fp, "div","Battery Voltage: %.3fV", sol_info->bat_v);
	webprintf(fp, "div","Battery charging amp: %.3fA", sol_info->bat_a);
	webprintf(fp, "div","Charging state: %s", sol_info->charging_state);
	fprintf(fp, "<div>State of Charge: %d%%</div>\n", sol_info->soc);
	fprintf(fp, "</div>\n");
	       
	webprintf(fp, "h2","Load Information");
	fprintf(fp, "<div class =\"grid-container\">\n");
	webprintf(fp, "div","Load Voltage: %.3fV", sol_info->load_v);
	webprintf(fp, "div","Load Current: %.3fA", sol_info->load_a);
	fprintf(fp, "</div>\n");
	
	webprintf(fp, "h2","Controller Information\n");
	fprintf(fp, "<div class =\"grid-container\">\n");
	webprintf(fp, "div","Device temperature: %dC", sol_info->device_temp);
	webprintf(fp, "div","system voltage setting: %dV", sol_info->system_voltage_setting);

	webprintf(fp, "div","system voltage recognized: %dV", sol_info->system_voltage_recognized);
	webprintf(fp, "div","System Max Voltage supported: %dV", sol_info->max_v_system);
	webprintf(fp, "div","System Rated Charge Current: %dA", sol_info->rated_charge_a);
	fprintf(fp, "</div>\n");
		  
	webprintf(fp, "h2","Battery History today");
	fprintf(fp, "<div class =\"grid-container\">");
	webprintf(fp, "div","Total battery charge: %dAH", sol_info->bat_charging_ah_today);
	webprintf(fp, "div","Total battery discharge: %dAH", sol_info->bat_discharging_ah_today);
	webprintf(fp, "div","Minimum battery voltage: %fV", sol_info->bat_min_volts_today);
	webprintf(fp, "div","Maximum battery voltage: %fV", sol_info->bat_max_volts_today);
	webprintf(fp, "div","Maximum battery<br>charging power: %fW", sol_info->bat_min_volts_today);
	fprintf(fp, "</div>\n");
		  
	webprintf(fp, "h2","Historical data");
	fprintf(fp, "<div class =\"grid-container\">");
	webprintf(fp, "div","Total Operating Days: %d", sol_info->total_operating_days);
	webprintf(fp, "div","Total times battery over discharged: %d",sol_info->bat_total_over_discharges);
	webprintf(fp, "div","Total times battery fully charged: %d", sol_info->bat_total_full_charges);
	fprintf(fp, "</div>\n");

	fprintf(fp, "</body>\n</html>\n");
	
	free_solar_info(sol_info);
}

void
web_history_status(FILE *fp, int day1, int day2)
{
	int day;
	SOLAR_INFO *sol_info;
	SOLAR_HISTORY *sol_history;
	
	sol_info = get_solar_info(modport);
	
	page_header(fp);
	fprintf(fp, "<div class=\"header\">\n");
	webprintf(fp, "h1","Solar History Status");
	webprintf(fp, "h2","Product Model: %s<br>"
		  "Hardware Version: %s<br>"
		  "Software Version: %s<br>"
		  "Serial number: %s</h2>",
		  sol_info->model,
		  sol_info->hardware_version,
		  sol_info->software_version,
		  sol_info->serial_number);
	free_solar_info(sol_info);
	
	fprintf(fp, "</div>\n");
	fprintf(fp, "<table>\n<tr>\n");
	webprintf(fp, "th","Day");
	webprintf(fp, "th","Min Battery Voltage(V)");
	webprintf(fp, "th","Max Battery Voltage(V)");
	webprintf(fp, "th","Max Charge Curr (A)");
	webprintf(fp, "th","Max Discharge Curr (A)");
	webprintf(fp, "th","Max Charge Power(W)");
	webprintf(fp, "th","Max Discharge Power(W)");
	webprintf(fp, "th","Charge(AH)");
	webprintf(fp, "th","Discharge(AH)");
	webprintf(fp, "th","Charge(KWH)");
	webprintf(fp, "th","Discharge(KWH)");
	
	fprintf(fp,"</tr>\n");

	prime_solar_history(modport, day1, day2);
	for (day = day1; day <= day2; day++) {
		sol_history = get_solar_history(day);
		fprintf(fp, "<tr>\n");
		webprintf(fp, "td","%d", sol_history->day);
		webprintf(fp, "td","%.3f", sol_history->bat_min_v);
		webprintf(fp, "td","%.3f", sol_history->bat_max_v);
		webprintf(fp, "td","%.3f", sol_history->bat_max_charge_a);
		webprintf(fp, "td","%.3f", sol_history->bat_max_discharge_a);
		webprintf(fp, "td","%.3f", sol_history->bat_max_charge_w);
		webprintf(fp, "td","%.3f", sol_history->bat_max_discharge_w);
		webprintf(fp, "td","%d", sol_history->bat_charge_ah);
		webprintf(fp, "td","%d", sol_history->bat_discharge_ah);
		webprintf(fp, "td","%.3f", sol_history->bat_charge_kwh);
		webprintf(fp, "td","%.3f", sol_history->bat_discharge_kwh);
		fprintf(fp,"</tr>\n");
		free_solar_history(sol_history);
	}
	fprintf(fp, "</table>\n");
	fprintf(fp, "</body>\n</html>\n");
}

/*
 * page_header
 * helper function with same page header is used for both status and history
 *
 * inputs:		File pointer to remote browser
 * output:		None
 * side effects:	prints HTML page header to remote browser
 */
static void
page_header(FILE *fp)
{
	fprintf(fp, "<html>\n<head>\n<style>\n"
	".grid-container {\n"
	"  display: grid;\n"
	"  grid-template-columns: auto auto auto auto;\n"
	"  grid-gap: 5px;\n"
	"  padding: 5px;\n"
	"  background-color: #2196F3;\n"
	"  font-size: 20px;\n"
	"}\n");
	fprintf(fp, "html {\n"
	       "font-family: \"Lucida sans\", sans-serif;\n"
	       "}\n");

	fprintf(fp, ".header {\n"
	       "background-color: #9933cc;\n"
	       "color: #ffffff;\n"
	       "padding: 15px;\n"
	       "}\n");

	fprintf(fp, "table, tr {\nborder: 1px solid black;\n}\n");
	fprintf(fp, "tr {\ntext-align: center;\n}\n");
	fprintf(fp, "</style>\n"
	       "</head>\n"
	       "<body>\n");
}

/*
 * prints given hdr with a <> around eg. <hdr> then the fmt
 * is scanned and trailing zeroes are removed. Finally adds
 * a terminating </hdr>
 */
static void
webprintf(FILE *fp, char *hdr, char *fmt, ...)
{
	va_list ap;
	char *ret;
	char *extra;
	
	va_start(ap, fmt);
	vasprintf(&ret, fmt, ap);
	extra = striptz(ret);
	if (extra != NULL)
		fprintf(fp, "<%s>%s%s</%s>\n", hdr, ret, extra, hdr);
	else
		fprintf(fp, "<%s>%s</%s>\n", hdr, ret, hdr);
	va_end(ap);
	free(ret);
}

/*
 * Surprisingly printf doesn't allow one to output %f removing trailing zeros
 * So a little routine here to do that. This allowed me to match
 * output with python and actually easier to read as well.
 *
 * Since digits is modified in place, I can return a pointer to the
 * end of the string which could be a 'V' or 'A' etc.
 */

static char *
striptz(char *digits)
{
	char *p;
	char *q;
	
	if ((p = strchr(digits, '.')) == NULL)
		return (NULL);
	p++;
	if ((q = strrchr(p, '0')) == NULL)
		return (NULL);
	p = q + 1;
	while (*q == '0' && q[1] != '.')
		*q-- = '\0';
	if (*p == '\0')
		p = NULL;
	return (p);
}
