/*
 * Copyright (c) 2024 Tenstorrent AI ULC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef REGULATOR_H
#define REGULATOR_H

#include <stdint.h>

#define P0V8_VCORE_ADDR  0x64
#define P0V8_VCOREM_ADDR 0x65

typedef enum {
	VoutCommand = 0,
	VoutMarginLow = 1,
	VoutMarginHigh = 2,
	AVSVoutCommand = 3,
} VoltageCmdSource;

float get_vcore(void);  /* returns voltage in mV. */
float get_vcorem(void); /* returns voltage in mV. */
void set_vcore(float voltage_in_mv);
void set_vcorem(float voltage_in_mv);
float GetVcoreCurrent(void);
float GetVcorePower(void);
void SwitchVoutControl(VoltageCmdSource source);
void RegulatorInit(void);
#endif
