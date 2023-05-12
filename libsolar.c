/* Copyright (c) 2022,2023 Diane Bruce
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
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <err.h>
#include <sysexits.h>

#include "libsolar.h"
#include "libmodbus.h"

/*
 * This library will read data from a Renogy controller
 * and return values that can be used by the web server
 * showing status or by remote_snap for database collection.
 */

DATA data_at_a[MAX_DATA];
DATA data_at_e001[MAX_DATA];
DATA data_at_100[MAX_DATA];

static DATA	access_data(ADDR);
static DATA	access_data_hi(ADDR i);
static DATA	access_data_lo(ADDR i);
static float	float_access_data(ADDR, float);
static int	access_long_data(ADDR);
static DATA	access_data_lo(ADDR);
static DATA	access_data_hi(ADDR);

/*
 * All data should be read via an accessor defined in this file
 */

SOLAR_SNAPSHOT *
get_solar_snapshot(const char *modport)
{
	int	fd;
	SOLAR_SNAPSHOT *status;

	status = malloc(sizeof(*status));
	if (NULL == status)
		return(NULL);

	fd = open_modbus(modport, B9600);
	if (fd < 0)
		err(EX_IOERR, "Can't open modbus %s", modport);
	read_registers(1, 35, 0x100, data_at_100);
	close(fd);
	status->array_v = float_access_data(PANEL_V, 10);
	status->array_a = float_access_data(PANEL_A, 100);
	status->array_w = access_data(CHARGING_POWER);
	status->soc = access_data(BAT_SOC);
	status->bat_v = float_access_data(BAT_V, 10);
	status->bat_a = float_access_data(BAT_CHARGING_AMP, 100);
	status->load_v = float_access_data(LOAD_V, 10);
	status->load_a = float_access_data(LOAD_A, 100);
	return (status);
}

static char *charging_names[] = {"Idle","Start","MPPT","EQU","BST","Float","Limit","Overcharge"};
static char *bat_type_names  [] = {"User","Flooded","Sealed","Gel","Lithium","Err","Err","Err"};

#define SMALL_BUF 20

SOLAR_INFO *
get_solar_info(const char *modport)
{
	char	model[20];
	char	hardware_version[SMALL_BUF];
	char	software_version[SMALL_BUF];
	char	serial_number[SMALL_BUF];	
	int	i,j;
	int	fd;
	int	fault_bits;
	SOLAR_INFO *info;

	fd = open_modbus(modport, B9600);
	if (fd < 0)
		err(EX_IOERR,"Can't open modbus %s", modport);

	/*
	 * These magic numbers, 17, 33 and 35
	 * come from a reverse engineered Windows program I examined. ;)
	 * N.B. These are not byte counts but short 16 bit word counts.
	 */
	read_registers(1, 17, 0xa, data_at_a);
	read_registers(1, 33, 0x100, data_at_100);
	read_registers(1, 35, 0xe001, data_at_e001);
	close(fd);
	
	info = malloc(sizeof(*info));
	if (NULL == info)
		return(NULL);

	/* Solar Panel Status */

	j = 0;
	for (i = MODEL_LO; i < MODEL_HI; i++) {
		model[j++] = access_data_hi(i) & 0xFF;
		model[j++] = access_data_lo(i) & 0xFF;
	}
	model[j] = '\0';
	info->model = strdup(model);
	
	sprintf(hardware_version, "%d.%d.%d",
		access_data_lo(HW_VERSION_LO),
		access_data_hi(HW_VERSION_HI),
		access_data_lo(HW_VERSION_HI));
	info->hardware_version = strdup(hardware_version);

	sprintf(software_version, "%d.%d.%d",
		access_data_lo(SW_VERSION_LO),		
		access_data_hi(SW_VERSION_HI),
		access_data_lo(SW_VERSION_HI));
	info->software_version = strdup(software_version);
	
	sprintf(serial_number, "%d%d%d%d",
		access_data_hi(SERIAL_NO_LO),
		access_data_lo(SERIAL_NO_LO),
		access_data_hi(SERIAL_NO_HI),
		access_data_lo(SERIAL_NO_HI));
	info->serial_number = strdup(serial_number);

	/* Array Information */
	info->array_v = float_access_data(PANEL_V,10);
	info->array_a = float_access_data(PANEL_A,100);
	info->array_w = access_data(CHARGING_POWER);
	fault_bits = access_long_data(CONTROLLER_FAULT_INFO);
	info->fault_bits = fault_bits;
	if (fault_bits & 0x100)
		info->array_working_state = strdup("Short Circuit");
	else if (fault_bits & 0x80)
		info->array_working_state = strdup("Over Power");
	else
		info->array_working_state = strdup("Normal");
	info->power_gen_today = access_data(POWER_GEN_TODAY);
	
	/* Battery Information */
	info->bat_v = float_access_data(BAT_V,10);
	info->bat_a = float_access_data(BAT_CHARGING_AMP,100);
	info->charging_state =
		strdup(charging_names[access_data(CHARGE_STATE) & 0x7]);
	info->bat_type = strdup(bat_type_names[access_data(BAT_INDEX) & 0x7]);
	info->bat_temp = access_data_lo(TEMPERATURE);
	info->soc = access_data(BAT_SOC);
	info->bat_capacity = access_data(BAT_CAPACITY);
	
	/* Load Information */
	info->load_v = float_access_data(LOAD_V,10);
	info->load_a = float_access_data(LOAD_A,100);

	/* Controller Information */
	info->device_temp = access_data_hi(TEMPERATURE);
	info->system_voltage_setting = access_data_hi(SYSTEM_VOLTAGE);
	info->system_voltage_recognized = access_data_lo(SYSTEM_VOLTAGE);
	info->max_v_system = access_data_hi(MAX_V_A);
	info->rated_charge_a = access_data_lo(MAX_V_A);
	
	/* Battery history today */
	info->bat_min_volts_today = float_access_data(BAT_MIN_V_TODAY,10);
	info->bat_max_volts_today = float_access_data(BAT_MAX_V_TODAY,10);
	info->bat_max_charge_a_today = 
		float_access_data(BAT_MAX_CHARGE_A_TODAY,100);
	info->bat_max_discharge_a_today = 
		float_access_data(BAT_MAX_DISCHARGE_A_TODAY,100);


	info->bat_charging_ah_today = access_data(BAT_CHARGING_AH_TODAY);
	info->bat_discharging_ah_today = access_data(BAT_DISCHARGING_AH_TODAY);
	info->bat_max_charging_power_today = 
		access_data(BAT_MAX_CHARGING_POWER_TODAY);

	/* Historical data */
	info->total_operating_days = access_data(TOTAL_OPERATING_DAYS);
	info->bat_total_over_discharges =
		access_data(BAT_TOTAL_OVER_DISCHARGES);
	info->bat_total_full_charges = access_data(BAT_TOTAL_FULL_CHARGES);

	return (info);
}


/*
 * History data per day on the Renogy controller is a 10 word
 * structure. Each day is addressed by one address which then returns
 * 10 words. 
 * This behaviour applies to each address >= 0xF000 to 0xFFFF
 * see renogy.h
 *
 * The idea here is to expand the data per day into an array
 * of 10 word values which makes it easier to retrieve SOLAR_HISTORY structures
 */

/* inputs	- name of serial port
 * 		- day1 index
 *		- day2
 * output	- none
 * side effects	- History array is filled in
 *
 * BUGS N.B. there is no way at present to ensure the history data
 * has been primed before accessed.
 */

DATA day_history[MAX_DAYS_HISTORY][MAX_DAY_DATA];

void
prime_solar_history(const char *modport, int day1, int day2)
{
	int day;
	int fd;

	fd = open_modbus(modport, B9600);
	if (fd < 0)
		err(EX_IOERR,"can't open modbus %s", modport);

	for (day = day1; day <= day2; day++) {
		read_registers(1, MAX_DAY_DATA, 0xF000 + day,
			       &day_history[day][0]);
	}
	close(fd);
}

/*
 * Given a request for a particular day history, index into
 * our array of solar_history items and fill in one SOLAR_HISTORY
 * to return.
 * N.B. at present prime_solar_history must be called first
 * However this is not enforced. (BUG)
 *
 * inputs	- day index into history array
 * output	- SOLAR_HISTORY
 * side effects	- none
 */

SOLAR_HISTORY *
get_solar_history(int day)
{
	SOLAR_HISTORY *solar_history;

	solar_history = malloc(sizeof(*solar_history));
	
	solar_history->day = day;
	solar_history->bat_min_v = (float)day_history[day][0] / 10.0;
	solar_history->bat_max_v = (float)day_history[day][1] / 10.0;
        solar_history->bat_max_charge_a = (float)day_history[day][2] / 100.0;
        solar_history->bat_max_discharge_a = (float)day_history[day][3] / 100.0;
        solar_history->bat_max_charge_w = (float)day_history[day][4] / 10.0;
        solar_history->bat_max_discharge_w = (float)day_history[day][5] / 10.0;
	solar_history->bat_charge_ah = day_history[day][6];
	solar_history->bat_discharge_ah = day_history[day][7];
	solar_history->bat_charge_kwh =	(float)day_history[day][8] / 1000.0;
        solar_history->bat_discharge_kwh = (float)day_history[day][9] / 1000.0;
	return (solar_history);
}

/*
 * SOLAR_INFO has dangling memory bits allocated
 * keep track of them to free
 */
 
void
free_solar_info(SOLAR_INFO *info)
{
       
	free(info->model);
	free(info->hardware_version);
	free(info->software_version);
	free(info->serial_number);
	free(info->array_working_state);
	free(info->charging_state);
	free(info->bat_type);
	free(info);
}

/*
 * For consistency
 */

void
free_solar_snapshot(SOLAR_SNAPSHOT *snap)
{
	free(snap);
}

/*
 * For consistency
 */

void
free_solar_history(SOLAR_HISTORY *history)
{
	free(history);
}

DATA
access_data(ADDR i)
{
	DATA *data;
	ADDR offset;
	
	if (i < 0x100) {
		offset = i - 0xa;
		data = data_at_a;
	} else if (i < 0xE000) {
		offset = i - 0x100;
		data = data_at_100;
	} else if (i < 0xF000) {
		offset = i - 0xE001;
		data = data_at_e001;
	}

      return (data[offset]);
}


static int
access_long_data(ADDR i)
{
	int	datahi;
	int	datalo;
	int	data;

	datahi = (access_data(i+1) << 16);
	datalo = (access_data(i) & 0xFFFF);
	data = datalo + datahi;
	return (data);
}

static DATA
access_data_lo(ADDR i)
{
	DATA data;

	data = access_data(i);
	return (data & 0xFF);
}

static DATA
access_data_hi(ADDR i)
{
	DATA data;

	data = access_data(i);
	return ((data >> 8) & 0xFF);
}

static float
float_access_data(ADDR i, float dp)
{

	return((float)access_data(i) / dp);
}

/*
 * get_csv_snapshot
 *
 * input	- char * to modport name
 * output	- char * of csv string generated from SOLAR_SNAPSHOT
 *
 * 
 */
char *
get_csv_snapshot(const char *modport)
{
	char *csv_line;
	SOLAR_SNAPSHOT *sol;
	struct tm *local_time;
	time_t cur_time;

	sol = get_solar_snapshot(modport);
	if (NULL == sol)
		return (NULL);

	time(&cur_time);
	local_time = localtime(&cur_time);
	asprintf(&csv_line,
		 "%4d-%02d-%02d %02d:%02d:%02d+00,%.1f,%.2f,%d,%d,%.2f,%.2f,%.2f,%.2f\n",
		 local_time->tm_year+1900, local_time->tm_mon + 1,
		 local_time->tm_mday, local_time->tm_hour,
		 local_time->tm_min, local_time->tm_sec,
		 sol->array_v, sol->array_a, sol->array_w,
		 sol->soc, sol->bat_v, sol->bat_a, 
		 sol->load_v, sol->load_a);
	return (csv_line);
}
