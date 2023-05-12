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
#ifndef __LIBSOLAR_H__
#define __LIBSOLAR_H__

#define MAX_DATA 40

typedef unsigned short ADDR;
typedef unsigned short DATA;

#define MODBUS_PORT_DEFAULT "/dev/cuaU0"

#include "renogy.h"

/*
 * struct of data items useful both for a webserver
 * and data collection for history database.
 */

typedef struct {
	float array_v;	/* solar array voltage */
	float array_a;	/* solar array amps */
	int array_w;	/* solar array watts */
	int soc;	/* state of charge of battery */
	float bat_v;	/* battery voltage */
	float bat_a;	/* battery amps */
	float load_v;	/* load voltage */
	float load_a;	/* load amps */
} SOLAR_SNAPSHOT;

typedef struct {
/* Solar Panel Status */
	char	*model;
	char	*hardware_version;
	char	*software_version;
	char	*serial_number;

/* Array Information */
	float	array_v;
	float	array_a;
	int	array_w;
	char	*array_working_state;
	int	power_gen_today;

/* Battery Information */
	float	bat_v;
	float	bat_a;
	char	*charging_state;
	char	*bat_type;
	int	bat_temp;
	int	soc;
	int	bat_capacity;

/* Load Information */
	float	load_v;
	float	load_a;
	
/* Controller Information */
	int	device_temp;
	int	system_voltage_setting;
	int	system_voltage_recognized;
	int	max_v_system;
	int	rated_charge_a;
	
/* Battery history today */
	float	bat_min_volts_today;
	float	bat_max_volts_today;
	int	bat_max_charge_a_today;
	int	bat_max_discharge_a_today;
	int	bat_max_charging_power_today;
	int	bat_max_discharge_power_today;
	int	bat_charging_ah_today;
	int	bat_discharging_ah_today;

/* Historical data */
	int	total_operating_days;
	int	bat_total_over_discharges;
	int	bat_total_full_charges;

/* Easy to present so why not ? */
	int	fault_bits;

} SOLAR_INFO;
	
typedef struct {
	int   day;
	float bat_min_v;
	float bat_max_v;
	float bat_max_charge_a;
	float bat_max_discharge_a;
	float bat_max_charge_w;
	float bat_max_discharge_w;
	int   bat_charge_ah;
	int   bat_discharge_ah;
	float bat_charge_kwh;
	float bat_discharge_kwh;
} SOLAR_HISTORY;

SOLAR_SNAPSHOT *get_solar_snapshot(const char *modport);
void	free_solar_snapshot(SOLAR_SNAPSHOT *snapshot);
SOLAR_INFO *get_solar_info(const char *modport);
void	prime_solar_history(const char *modport, int day1, int day2);
SOLAR_HISTORY *get_solar_history(int day);
void	free_solar_info(SOLAR_INFO *info);
void	free_solar_history(SOLAR_HISTORY *history);
char*	get_csv_snapshot(const char *modport);


#endif
