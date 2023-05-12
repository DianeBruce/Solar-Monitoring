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

#ifndef _LIBMODBUS_H_
#define _LIBMODBUS_H_

#define READ_COIL_STATUS	1
#define READ_INPUT_STATUS	2
#define READ_HOLDING_REGISTERS	3

#define READ_INPUT_REGISTERS	4
#define WRITE_SINGLE_COIL	5
#define WRITE_SINGLE_REGISTER	6
#define WRITE_MULTIPLE_COILS	15
#define WRITE_MULTIPLE_REGISTERS 16

#include <termios.h>
int open_modbus(const char *tty_name, speed_t speed);
int close_modbus(int fd);
int write_registers(int device_id, int count,
		    unsigned short addr, unsigned short data[]);
int read_registers(int device_id, int count,
		   unsigned short addr, unsigned short data[]);


#endif
