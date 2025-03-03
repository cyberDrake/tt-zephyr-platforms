/*
 * Copyright (c) 2024 Tenstorrent AI ULC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "regulator.h"

#include <math.h>  /* for ldexp */
#include <float.h> /* for FLT_MAX */
#include <stdint.h>
#include <zephyr/kernel.h>
#include <tenstorrent/msg_type.h>
#include <tenstorrent/msgqueue.h>

#include "timer.h"
#include "dw_apb_i2c.h"
#include "avs.h"

/* I2C constants */
#define PMBUS_MST_ID 1

/* PMBus Spec constants */
#define VOUT_COMMAND                0x21
#define VOUT_COMMAND_DATA_BYTE_SIZE 2
#define READ_VOUT                   0x8B
#define READ_VOUT_DATA_BYTE_SIZE    2
#define READ_IOUT                   0x8C
#define READ_IOUT_DATA_BYTE_SIZE    2
#define READ_POUT                   0x96
#define READ_POUT_DATA_BYTE_SIZE    2
#define OPERATION                   0x1
#define OPERATION_DATA_BYTE_SIZE    1
#define PMBUS_CMD_BYTE_SIZE         1
#define PMBUS_FLIP_BYTES            0

typedef struct {
	uint8_t reserved: 1;
	uint8_t transition_control: 1;
	uint8_t margin_fault_response: 2;

	VoltageCmdSource voltage_command_source : 2;
	uint8_t turn_off_behaviour: 1;
	uint8_t on_off_state: 1;
} OperationBits;

/* The default value is the regulator default */
static uint8_t vout_cmd_source = VoutCommand;

static float ConvertLinear11ToFloat(uint16_t value)
{
	int16_t exponent = (value >> 11) & 0x1f;
	uint16_t mantissa = value & 0x7ff;

	if (exponent >> 4 == 1) { /* sign extension if negative */
		exponent |= ~0x1F;
	}

	return ldexp(mantissa, exponent);
}

/* The function returns the core current in A. */
float GetVcoreCurrent(void)
{
	I2CInit(I2CMst, P0V8_VCORE_ADDR, I2CFastMode, PMBUS_MST_ID);
	uint16_t iout;

	I2CReadBytes(PMBUS_MST_ID, READ_IOUT, PMBUS_CMD_BYTE_SIZE, (uint8_t *)&iout,
		     READ_IOUT_DATA_BYTE_SIZE, PMBUS_FLIP_BYTES);
	return ConvertLinear11ToFloat(iout);
}

/* The function returns the core power in W. */
float GetVcorePower(void)
{
	I2CInit(I2CMst, P0V8_VCORE_ADDR, I2CFastMode, PMBUS_MST_ID);
	uint16_t pout;

	I2CReadBytes(PMBUS_MST_ID, READ_POUT, PMBUS_CMD_BYTE_SIZE, (uint8_t *)&pout,
		     READ_POUT_DATA_BYTE_SIZE, PMBUS_FLIP_BYTES);
	return ConvertLinear11ToFloat(pout);
}

/* Set MAX20816 voltage using I2C, MAX20816 is used for Vcore and Vcorem */
void i2c_set_max20816(uint32_t slave_addr, float voltage_in_mv)
{
	I2CInit(I2CMst, slave_addr, I2CFastMode, PMBUS_MST_ID);
	uint16_t vout_cmd = 2 * voltage_in_mv;

	I2CWriteBytes(PMBUS_MST_ID, VOUT_COMMAND, PMBUS_CMD_BYTE_SIZE, (uint8_t *)&vout_cmd,
		      VOUT_COMMAND_DATA_BYTE_SIZE);

	/* 100us to flush the tx of i2c + 150us to cover voltage switch from 0.65V to 0.95V with
	 * 50us of margin
	 */
	WaitUs(250);
}

/* Returns MAX20816 output volage in mV. */
float i2c_get_max20816(uint32_t slave_addr)
{
	I2CInit(I2CMst, slave_addr, I2CFastMode, PMBUS_MST_ID);
	uint16_t vout_cmd = 0;

	I2CReadBytes(PMBUS_MST_ID, READ_VOUT, PMBUS_CMD_BYTE_SIZE, (uint8_t *)&vout_cmd,
		     READ_VOUT_DATA_BYTE_SIZE, PMBUS_FLIP_BYTES);

	return vout_cmd * 0.5f;
}

void set_vcore(float voltage_in_mv)
{
	if (vout_cmd_source == AVSVoutCommand) {
		AVSWriteVoltage(voltage_in_mv, AVS_VCORE_RAIL);
	} else {
		i2c_set_max20816(P0V8_VCORE_ADDR, voltage_in_mv);
	}
}

float get_vcore(void)
{
	return i2c_get_max20816(P0V8_VCORE_ADDR);
}

void set_vcorem(float voltage_in_mv)
{
	i2c_set_max20816(P0V8_VCOREM_ADDR, voltage_in_mv);
}

float get_vcorem(void)
{
	return i2c_get_max20816(P0V8_VCOREM_ADDR);
}

void SwitchVoutControl(VoltageCmdSource source)
{
	I2CInit(I2CMst, P0V8_VCORE_ADDR, I2CFastMode, PMBUS_MST_ID);
	OperationBits operation;

	I2CReadBytes(PMBUS_MST_ID, OPERATION, PMBUS_CMD_BYTE_SIZE, (uint8_t *)&operation,
		     OPERATION_DATA_BYTE_SIZE, PMBUS_FLIP_BYTES);
	operation.transition_control =
		1; /* copy vout command when control is passed from AVSBus to PMBus */
	operation.voltage_command_source = source;
	I2CWriteBytes(PMBUS_MST_ID, OPERATION, PMBUS_CMD_BYTE_SIZE, (uint8_t *)&operation,
		      OPERATION_DATA_BYTE_SIZE);

	/* 100us to flush the tx of i2c */
	WaitUs(100);
	vout_cmd_source = source;
}

void RegulatorInit(void)
{
	/* VCORE */
	I2CInit(I2CMst, P0V8_VCORE_ADDR, I2CFastMode, PMBUS_MST_ID);

	static const uint8_t data_b0[] = {0x15, 0x09, 0x3c, 0x08, 0x0a, 0x02, 0x0f, 0x00,
					  0x11, 0x00, 0x00, 0x00, 0x00, 0x41, 0x03, 0x00,
					  0x00, 0x0f, 0x0d, 0x0a, 0x00, 0x00};
	static const uint8_t data_ca[] = {0x04, 0x78, 0x3c, 0x0f, 0x00};
	static const uint8_t data_cb[] = {0x05, 0x50, 0x0e, 0x64, 0x28, 0x00};
	static const uint8_t data_d3[] = {0x00};
	static const uint8_t data_38[] = {0x08, 0x00};
	static const uint8_t data_39[] = {0x0c, 0x00};
	static const uint8_t data_e7[] = {0x01};

	I2CWriteBytes(PMBUS_MST_ID, 0xb0, PMBUS_CMD_BYTE_SIZE, data_b0, sizeof(data_b0));
	I2CWriteBytes(PMBUS_MST_ID, 0xca, PMBUS_CMD_BYTE_SIZE, data_ca, sizeof(data_ca));
	I2CWriteBytes(PMBUS_MST_ID, 0xcb, PMBUS_CMD_BYTE_SIZE, data_cb, sizeof(data_cb));
	I2CWriteBytes(PMBUS_MST_ID, 0xd3, PMBUS_CMD_BYTE_SIZE, data_d3, sizeof(data_d3));
	I2CWriteBytes(PMBUS_MST_ID, 0x38, PMBUS_CMD_BYTE_SIZE, data_38, sizeof(data_38));
	I2CWriteBytes(PMBUS_MST_ID, 0x39, PMBUS_CMD_BYTE_SIZE, data_39, sizeof(data_39));
	I2CWriteBytes(PMBUS_MST_ID, 0xe7, PMBUS_CMD_BYTE_SIZE, data_e7, sizeof(data_e7));

	/* VCOREM */
	static const uint8_t data2_b0[] = {0x0f, 0x19, 0x2b, 0x08, 0x17, 0x07, 0x0f, 0x00,
					   0x09, 0x63, 0x09, 0x00, 0x00, 0x3f, 0x3d, 0x3a};
	static const uint8_t data2_38[] = {0x08, 0x00};
	static const uint8_t data2_39[] = {0x0c, 0x00};
	static const uint8_t data2_e7[] = {0x10};

	I2CInit(I2CMst, P0V8_VCOREM_ADDR, I2CFastMode, PMBUS_MST_ID);
	I2CWriteBytes(PMBUS_MST_ID, 0xb0, PMBUS_CMD_BYTE_SIZE, data2_b0, sizeof(data2_b0));
	I2CWriteBytes(PMBUS_MST_ID, 0x38, PMBUS_CMD_BYTE_SIZE, data2_38, sizeof(data2_38));
	I2CWriteBytes(PMBUS_MST_ID, 0x39, PMBUS_CMD_BYTE_SIZE, data2_39, sizeof(data2_39));
	I2CWriteBytes(PMBUS_MST_ID, 0xe7, PMBUS_CMD_BYTE_SIZE, data2_e7, sizeof(data2_e7));
}

static uint8_t set_voltage_handler(uint32_t msg_code, const struct request *request,
				   struct response *response)
{
	uint32_t slave_addr = request->data[1];
	float voltage_in_mv = request->data[2];

	if (slave_addr != P0V8_VCORE_ADDR && slave_addr != P0V8_VCOREM_ADDR) {
		return 1;
	}

	if (slave_addr == P0V8_VCORE_ADDR) {
		set_vcore(voltage_in_mv);
	} else if (slave_addr == P0V8_VCOREM_ADDR) {
		set_vcorem(voltage_in_mv);
	}

	return 0;
}

static uint8_t get_voltage_handler(uint32_t msg_code, const struct request *request,
				   struct response *response)
{
	uint32_t slave_addr = request->data[1];

	if (slave_addr != P0V8_VCORE_ADDR && slave_addr != P0V8_VCOREM_ADDR) {
		return 1;
	}

	if (slave_addr == P0V8_VCORE_ADDR) {
		response->data[1] = get_vcore();
	} else if (slave_addr == P0V8_VCOREM_ADDR) {
		response->data[1] = get_vcorem();
	}

	return 0;
}

static uint8_t switch_vout_control_handler(uint32_t msg_code, const struct request *request,
					   struct response *response)
{
	uint32_t source = request->data[1];

	SwitchVoutControl(source);
	return 0;
}

REGISTER_MESSAGE(MSG_TYPE_SET_VOLTAGE, set_voltage_handler);
REGISTER_MESSAGE(MSG_TYPE_GET_VOLTAGE, get_voltage_handler);
REGISTER_MESSAGE(MSG_TYPE_SWITCH_VOUT_CONTROL, switch_vout_control_handler);
