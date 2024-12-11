// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2020 Samsung Electronics Co., Ltd.
 * Copyright 2020 Google LLC.
 * Copyright 2024 Linaro Ltd.
 */
#include <linux/bitfield.h>
#include <linux/firmware/samsung/exynos-acpm-protocol.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/types.h>

#include "exynos-acpm.h"
#include "exynos-acpm-pmic.h"

#define ACPM_PMIC_CHANNEL		GENMASK(15, 12)
#define ACPM_PMIC_TYPE			GENMASK(11, 8)
#define ACPM_PMIC_REG			GENMASK(7, 0)

#define ACPM_PMIC_RETURN		GENMASK(31, 24)
#define ACPM_PMIC_MASK			GENMASK(23, 16)
#define ACPM_PMIC_VALUE			GENMASK(15, 8)
#define ACPM_PMIC_FUNC			GENMASK(7, 0)

#define ACPM_BULK_SHIFT			8
#define ACPM_BULK_MASK			GENMASK(7, 0)

enum exynos_acpm_pmic_func {
	ACPM_PMIC_READ,
	ACPM_PMIC_WRITE,
	ACPM_PMIC_UPDATE,
	ACPM_PMIC_BULK_READ,
	ACPM_PMIC_BULK_WRITE,
};

static inline u32 exynos_acpm_set_bulk(u32 data, unsigned int i)
{
	return (data & ACPM_BULK_MASK) << (ACPM_BULK_SHIFT * i);
}

static inline u32 exynos_acpm_read_bulk(u32 data, unsigned int i)
{
	return (data >> (ACPM_BULK_SHIFT * i)) & ACPM_BULK_MASK;
}

static void acpm_pmic_init_read_cmd(u32 *cmd, u8 type, u8 reg, u8 chan)
{
	cmd[0] = FIELD_PREP(ACPM_PMIC_TYPE, type) |
		 FIELD_PREP(ACPM_PMIC_REG, reg) |
		 FIELD_PREP(ACPM_PMIC_CHANNEL, chan);
	cmd[1] = FIELD_PREP(ACPM_PMIC_FUNC, ACPM_PMIC_READ);
	cmd[3] = (u32)(ktime_to_ms(ktime_get()));
}

int acpm_pmic_read_reg(const struct acpm_handle *handle, int acpm_chan_id,
		       u8 type, u8 reg, u8 chan, u8 *dest)
{
	struct acpm_xfer xfer;
	u32 cmd[4] = {0};
	int ret;

	acpm_pmic_init_read_cmd(cmd, type, reg, chan);

	xfer.tx.cmd = cmd;
	xfer.tx.len = sizeof(cmd);
	xfer.rx.cmd = cmd;
	xfer.rx.len = sizeof(cmd);
	xfer.acpm_chan_id = acpm_chan_id;

	ret = acpm_do_xfer(handle, &xfer);
	if (ret)
		return ret;

	*dest = FIELD_GET(ACPM_PMIC_VALUE, xfer.rx.cmd[1]);

	return FIELD_GET(ACPM_PMIC_RETURN, xfer.rx.cmd[1]);
}

static void acpm_pmic_init_bulk_read_cmd(u32 *cmd, u8 type, u8 reg, u8 chan,
					 u8 count)
{
	cmd[0] = FIELD_PREP(ACPM_PMIC_TYPE, type) |
		 FIELD_PREP(ACPM_PMIC_REG, reg) |
		 FIELD_PREP(ACPM_PMIC_CHANNEL, chan);
	cmd[1] = FIELD_PREP(ACPM_PMIC_FUNC, ACPM_PMIC_BULK_READ) |
		 FIELD_PREP(ACPM_PMIC_VALUE, count);
}

int acpm_pmic_bulk_read(const struct acpm_handle *handle, int acpm_chan_id,
			u8 type, u8 reg, u8 chan, u8 count, u8 *buf)
{
	struct acpm_xfer xfer;
	u32 cmd[4] = {0};
	int i, ret;

	acpm_pmic_init_bulk_read_cmd(cmd, type, reg, chan, count);

	xfer.tx.cmd = cmd;
	xfer.tx.len = sizeof(cmd);
	xfer.rx.cmd = cmd;
	xfer.rx.len = sizeof(cmd);
	xfer.acpm_chan_id = acpm_chan_id;

	ret = acpm_do_xfer(handle, &xfer);
	if (ret)
		return ret;

	ret = FIELD_GET(ACPM_PMIC_RETURN, xfer.rx.cmd[1]);
	if (ret)
		return ret;

	for (i = 0; i < count; i++) {
		if (i < 4)
			buf[i] = exynos_acpm_read_bulk(xfer.rx.cmd[2], i);
		else
			buf[i] = exynos_acpm_read_bulk(xfer.rx.cmd[3], i - 4);
	}

	return 0;
}

static void acpm_pmic_init_write_cmd(u32 *cmd, u8 type, u8 reg, u8 chan,
				     u8 value)
{
	cmd[0] = FIELD_PREP(ACPM_PMIC_TYPE, type) |
		 FIELD_PREP(ACPM_PMIC_REG, reg) |
		 FIELD_PREP(ACPM_PMIC_CHANNEL, chan);
	cmd[1] = FIELD_PREP(ACPM_PMIC_FUNC, ACPM_PMIC_WRITE) |
		 FIELD_PREP(ACPM_PMIC_VALUE, value);
	cmd[3] = (u32)(ktime_to_ms(ktime_get()));
}

int acpm_pmic_write_reg(const struct acpm_handle *handle, int acpm_chan_id,
			u8 type, u8 reg, u8 chan, u8 value)
{
	struct acpm_xfer xfer;
	u32 cmd[4] = {0};
	int ret;

	acpm_pmic_init_write_cmd(cmd, type, reg, chan, value);

	xfer.tx.cmd = cmd;
	xfer.tx.len = sizeof(cmd);
	xfer.rx.cmd = cmd;
	xfer.rx.len = sizeof(cmd);
	xfer.acpm_chan_id = acpm_chan_id;

	ret = acpm_do_xfer(handle, &xfer);
	if (ret)
		return ret;

	return FIELD_GET(ACPM_PMIC_RETURN, xfer.rx.cmd[1]);
}

static void acpm_pmic_init_bulk_write_cmd(u32 *cmd, u8 type, u8 reg, u8 chan,
					  u8 count, u8 *buf)
{
	int i;

	cmd[0] = FIELD_PREP(ACPM_PMIC_TYPE, type) |
		 FIELD_PREP(ACPM_PMIC_REG, reg) |
		 FIELD_PREP(ACPM_PMIC_CHANNEL, chan);
	cmd[1] = FIELD_PREP(ACPM_PMIC_FUNC, ACPM_PMIC_BULK_WRITE) |
		 FIELD_PREP(ACPM_PMIC_VALUE, count);

	for (i = 0; i < count; i++) {
		if (i < 4)
			cmd[2] |= exynos_acpm_set_bulk(buf[i], i);
		else
			cmd[3] |= exynos_acpm_set_bulk(buf[i], i - 4);
	}
}

int acpm_pmic_bulk_write(const struct acpm_handle *handle, int acpm_chan_id,
			 u8 type, u8 reg, u8 chan, u8 count, u8 *buf)
{
	struct acpm_xfer xfer;
	u32 cmd[4] = {0};
	int ret;

	acpm_pmic_init_bulk_write_cmd(cmd, type, reg, chan, count, buf);

	xfer.tx.cmd = cmd;
	xfer.tx.len = sizeof(cmd);
	xfer.rx.cmd = cmd;
	xfer.rx.len = sizeof(cmd);
	xfer.acpm_chan_id = acpm_chan_id;

	ret = acpm_do_xfer(handle, &xfer);
	if (ret)
		return ret;

	return FIELD_GET(ACPM_PMIC_RETURN, xfer.rx.cmd[1]);
}

static void acpm_pmic_init_update_cmd(u32 *cmd, u8 type, u8 reg, u8 chan,
				      u8 value, u8 mask)
{
	cmd[0] = FIELD_PREP(ACPM_PMIC_TYPE, type) |
		 FIELD_PREP(ACPM_PMIC_REG, reg) |
		 FIELD_PREP(ACPM_PMIC_CHANNEL, chan);
	cmd[1] = FIELD_PREP(ACPM_PMIC_FUNC, ACPM_PMIC_UPDATE) |
		 FIELD_PREP(ACPM_PMIC_VALUE, value) |
		 FIELD_PREP(ACPM_PMIC_MASK, mask);
	cmd[3] = (u32)(ktime_to_ms(ktime_get()));
}

int acpm_pmic_update_reg(const struct acpm_handle *handle, int acpm_chan_id,
			 u8 type, u8 reg, u8 chan, u8 value, u8 mask)
{
	struct acpm_xfer xfer;
	u32 cmd[4] = {0};
	int ret;

	acpm_pmic_init_update_cmd(cmd, type, reg, chan, value, mask);

	xfer.tx.cmd = cmd;
	xfer.tx.len = sizeof(cmd);
	xfer.rx.cmd = cmd;
	xfer.rx.len = sizeof(cmd);
	xfer.acpm_chan_id = acpm_chan_id;

	ret = acpm_do_xfer(handle, &xfer);
	if (ret)
		return ret;

	return FIELD_GET(ACPM_PMIC_RETURN, xfer.rx.cmd[1]);
}

MODULE_AUTHOR("Tudor Ambarus <tudor.ambarus@linaro.org>");
MODULE_DESCRIPTION("Samsung Exynos ACPM mailbox PMIC protocol driver");
MODULE_LICENSE("GPL");
