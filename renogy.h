/* Copyright (c) 2019,2020,2021,2022,2023 Diane Bruce
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

#ifndef __RENOGY_H__
#define __RENOGY_H__

#define MODEL_LO 0xC		/* Model is an ASCII array not nul terminated */
#define MODEL_HI 0x13

#define SW_VERSION_LO 0x14	/* Versions are 3 bytes each */
#define SW_VERSION_HI 0x15
#define HW_VERSION_LO 0x16
#define HW_VERSION_HI 0x17

#define SERIAL_NO_LO	0x18		/* Serial number */
#define SERIAL_NO_HI	0x19
#define MAX_V_A		0xA
#define MAX_DISCHARGE_A_TYPE 0xB
#define BAT_SOC		0x100		/* Battery state of charge */
#define BAT_V		0x101		/* Battery voltage */
#define BAT_CHARGING_AMP 0x102
#define TEMPERATURE	0x103		/* Temperature device/battery */
#define LOAD_V		0x104		/* Load voltage */
#define LOAD_A		0x105		/* Load amps */
#define PANEL_V		0x107		/* Voltage from solar array */
#define PANEL_A		0x108		/* Amps from solar array */
#define CHARGING_POWER	0x109
#define BAT_MIN_V_TODAY	0x10B		/* Battery minimum voltage today */
#define BAT_MAX_V_TODAY	0x10C
#define BAT_MAX_CHARGE_A_TODAY	0x10D
#define BAT_MAX_DISCHARGE_A_TODAY  0x10E
#define BAT_MAX_CHARGING_POWER_TODAY	0x10F
#define BAT_MAX_DISCHARGING_POWER_TODAY  0x110
#define BAT_CHARGING_AH_TODAY		0x111
#define BAT_DISCHARGING_AH_TODAY	0x112
#define POWER_GEN_TODAY 	 0x113
#define POWER_CONSUMPTION_TODAY  0x114
#define TOTAL_OPERATING_DAYS	0x115
#define BAT_TOTAL_OVER_DISCHARGES  0x116
#define BAT_TOTAL_FULL_CHARGES	0x117
#define BAT_MAX_CHARGING_POWER  0x118
#define CUMULATIVE_POWER_GENERATION  0x11C
#define CUMULATIVE_POWER_CONSUMPTION  0x11E
#define CHARGE_STATE  0x120			/* floating, MPPT etc. */
#define CONTROLLER_FAULT_INFO  0x121		/* Bit map of faults */

#define BAT_CAPACITY	0xE002
#define SYSTEM_VOLTAGE  0xE003
#define BAT_INDEX	0xE004

#define	MAX_DAY_DATA	10	/* Renogy always returns 10 words per day */

#define MAX_DAYS_HISTORY	204

#endif
