/* OK, info extracted from a running .32 kernel */
/*
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com>
 * Copyright (C) 2010 Google, Inc.
 *
 * Authors:
 *	Colin Cross <ccross@google.com>
 *	Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Tegra2 external memory controller initialization */
 
#include <linux/kernel.h>
#include <linux/init.h>

#include "board-shuttle.h"
#include "tegra2_emc.h"

#if defined (SHUTTLE_EMC_SAMSUNG)

/* Samsung: manufacturer_id = 0x0101, mem_pid = 0x5454 */
static const struct tegra_emc_table shuttle_emc_tables[] = {
	{
		.rate = 25000,   /* SDRAM frequency */
		.regs = {
			0x00000002,   /* RC */
			0x00000006,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000004,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000004d,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000004,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000068,   /* TREFBW */
			0x00000003,   /* QUSE_EXTRA */
			0x00000003,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa09404ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000003,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 50000,   /* SDRAM frequency */
		.regs = {
			0x00000003,   /* RC */
			0x00000007,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000009f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000007,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000000d0,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa09404ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000005,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 75000,   /* SDRAM frequency */
		.regs = {
			0x00000005,   /* RC */
			0x0000000a,   /* RFC */
			0x00000004,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x000000ff,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x0000000b,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000138,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa09404ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000007,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 150000,   /* SDRAM frequency */
		.regs = {
			0x00000009,   /* RC */
			0x00000014,   /* RFC */
			0x00000007,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000021f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000015,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000270,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000001,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa06804ae,   /* CFG_DIG_DLL */
			0x007e0010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000000e,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 300000,   /* SDRAM frequency */
		.regs = {
			0x00000012,   /* RC */
			0x00000027,   /* RFC */
			0x0000000d,   /* RAS */
			0x00000006,   /* RP */
			0x00000007,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x00000009,   /* W2P */
			0x00000006,   /* RD_RCD */
			0x00000006,   /* WR_RCD */
			0x00000003,   /* RRD */
			0x00000003,   /* REXT */
			0x00000002,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x0000045f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000006,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000e,   /* RW2PDEN */
			0x0000002a,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000f,   /* TFAW */
			0x00000007,   /* TRPAB */
			0x00000005,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000004e0,   /* TREFBW */
			0x00000005,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xe04e048b,   /* CFG_DIG_DLL */
			0x007e0010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000001b,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	}
};

#elif defined (SHUTTLE_EMC_ELPIDA50NM)

/* Elpida 50nm: .mem_manufacturer_id = 0x0303, .mem_pid = 0x5454 */
static const struct tegra_emc_table shuttle_emc_tables[] = {
	{
		.rate = 25000,   /* SDRAM frequency */
		.regs = {
			0x00000002,   /* RC */
			0x00000006,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000004,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000004d,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000004,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000068,   /* TREFBW */
			0x00000003,   /* QUSE_EXTRA */
			0x00000003,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa06a04ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000003,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 50000,   /* SDRAM frequency */
		.regs = {
			0x00000003,   /* RC */
			0x00000007,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000009f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000007,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000000d0,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa06a04ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000005,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 75000,   /* SDRAM frequency */
		.regs = {
			0x00000005,   /* RC */
			0x0000000a,   /* RFC */
			0x00000004,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x000000ff,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x0000000b,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000138,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa06a04ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000007,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 150000,   /* SDRAM frequency */
		.regs = {
			0x00000009,   /* RC */
			0x00000014,   /* RFC */
			0x00000007,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000021f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000015,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000270,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000001,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa04c04ae,   /* CFG_DIG_DLL */
			0x007e0010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000000e,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 300000,   /* SDRAM frequency */
		.regs = {
			0x00000012,   /* RC */
			0x00000027,   /* RFC */
			0x0000000d,   /* RAS */
			0x00000006,   /* RP */
			0x00000007,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x00000009,   /* W2P */
			0x00000006,   /* RD_RCD */
			0x00000006,   /* WR_RCD */
			0x00000003,   /* RRD */
			0x00000003,   /* REXT */
			0x00000002,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x0000045f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000006,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000e,   /* RW2PDEN */
			0x0000002a,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000f,   /* TFAW */
			0x00000007,   /* TRPAB */
			0x00000005,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000004e0,   /* TREFBW */
			0x00000005,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xe03c048b,   /* CFG_DIG_DLL */
			0x007e0010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000001b,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	}
};

#elif defined (SHUTTLE_EMC_ELPIDA40NM)

/* Elpida 40nm : mem_manufacturer_id = 0x0303, mem_revision_id1 = 0x0101, mem_pid = 0x5454 */
static const struct tegra_emc_table shuttle_emc_tables[] = {
	{
		.rate = 25000,   /* SDRAM frequency */
		.regs = {
			0x00000002,   /* RC */
			0x00000006,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000004,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000004d,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000004,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000068,   /* TREFBW */
			0x00000003,   /* QUSE_EXTRA */
			0x00000003,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa0ae04ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000003,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 50000,   /* SDRAM frequency */
		.regs = {
			0x00000003,   /* RC */
			0x00000007,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000009f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000007,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000000d0,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa0ae04ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000005,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 75000,   /* SDRAM frequency */
		.regs = {
			0x00000005,   /* RC */
			0x0000000a,   /* RFC */
			0x00000004,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x000000ff,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x0000000b,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000138,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa0ae04ae,   /* CFG_DIG_DLL */
			0x00070000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000007,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 150000,   /* SDRAM frequency */
		.regs = {
			0x00000009,   /* RC */
			0x00000014,   /* RFC */
			0x00000007,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000021f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000015,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000270,   /* TREFBW */
			0x00000004,   /* QUSE_EXTRA */
			0x00000001,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xa07c04ae,   /* CFG_DIG_DLL */
			0x007de010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000000e,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 300000,   /* SDRAM frequency */
		.regs = {
			0x00000012,   /* RC */
			0x00000027,   /* RFC */
			0x0000000d,   /* RAS */
			0x00000006,   /* RP */
			0x00000007,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x00000009,   /* W2P */
			0x00000006,   /* RD_RCD */
			0x00000006,   /* WR_RCD */
			0x00000003,   /* RRD */
			0x00000003,   /* REXT */
			0x00000002,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x0000045f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000006,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000e,   /* RW2PDEN */
			0x0000002a,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000f,   /* TFAW */
			0x00000007,   /* TRPAB */
			0x00000005,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000004e0,   /* TREFBW */
			0x00000005,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xe059048b,   /* CFG_DIG_DLL */
			0x007e4010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000001b,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	}
};

#else

/* This is the LPDDR2 sdram used in Shuttle: Hynax HY5PS1G831C-S5 */
static const struct tegra_emc_table shuttle_emc_tables[] = {
	{
		.rate = 166500,   /* SDRAM frequency */
		.regs = {
            0x0000000A,   /* RC */
            0x00000016,   /* RFC */
            0x00000008,   /* RAS */
            0x00000003,   /* RP */
            0x00000004,   /* R2W */
            0x00000004,   /* W2R */
            0x00000002,   /* R2P */
            0x0000000C,   /* W2P */
            0x00000003,   /* RD_RCD */
            0x00000003,   /* WR_RCD */
            0x00000002,   /* RRD */
            0x00000001,   /* REXT */
            0x00000004,   /* WDV */
            0x00000005,   /* QUSE */
            0x00000004,   /* QRST */
            0x00000009,   /* QSAFE */
            0x0000000D,   /* RDV */
            0x000004DF,   /* REFRESH */
            0x00000000,   /* BURST_REFRESH_NUM */
            0x00000003,   /* PDEX2WR */
            0x00000003,   /* PDEX2RD */
            0x00000003,   /* PCHG2PDEN */
            0x00000003,   /* ACT2PDEN */
            0x00000001,   /* AR2PDEN */
            0x0000000A,   /* RW2PDEN */
            0x000000C8,   /* TXSR */
            0x00000003,   /* TCKE */
            0x00000006,   /* TFAW */
            0x00000004,   /* TRPAB */
            0x00000008,   /* TCLKSTABLE */
            0x00000002,   /* TCLKSTOP */
            0x00000000,   /* TREFBW */
            0x00000000,   /* QUSE_EXTRA */
            0x00000002,   /* FBIO_CFG6 */
            0x00000000,   /* ODT_WRITE */
            0x00000000,   /* ODT_READ */
            0x00000083,   /* FBIO_CFG5 */
            0xA04204AE,   /* CFG_DIG_DLL */
            0x007FD010,   /* DLL_XFORM_DQS */
            0x00000000,   /* DLL_XFORM_QUSE */
            0x00000000,   /* ZCAL_REF_CNT */
            0x00000000,   /* ZCAL_WAIT_CNT */
            0x00000000,   /* AUTO_CAL_INTERVAL */
            0x00000000,   /* CFG_CLKTRIM_0 */
            0x00000000,   /* CFG_CLKTRIM_1 */
            0x00000000,   /* CFG_CLKTRIM_2 */
        }
    },
	{
		.rate = 333000,   /* SDRAM frequency */
		.regs = {
            0x00000014,   /* RC */
            0x0000002B,   /* RFC */
            0x0000000F,   /* RAS */
            0x00000005,   /* RP */
            0x00000004,   /* R2W */
            0x00000005,   /* W2R */
            0x00000003,   /* R2P */
            0x0000000C,   /* W2P */
            0x00000005,   /* RD_RCD */
            0x00000005,   /* WR_RCD */
            0x00000003,   /* RRD */
            0x00000001,   /* REXT */
            0x00000004,   /* WDV */
            0x00000005,   /* QUSE */
            0x00000004,   /* QRST */
            0x00000009,   /* QSAFE */
            0x0000000D,   /* RDV */
            0x000009FF,   /* REFRESH */
            0x00000000,   /* BURST_REFRESH_NUM */
            0x00000003,   /* PDEX2WR */
            0x00000003,   /* PDEX2RD */
            0x00000005,   /* PCHG2PDEN */
            0x00000005,   /* ACT2PDEN */
            0x00000001,   /* AR2PDEN */
            0x0000000F,   /* RW2PDEN */
            0x000000C8,   /* TXSR */
            0x00000003,   /* TCKE */
            0x0000000C,   /* TFAW */
            0x00000006,   /* TRPAB */
            0x00000008,   /* TCLKSTABLE */
            0x00000002,   /* TCLKSTOP */
            0x00000000,   /* TREFBW */
            0x00000000,   /* QUSE_EXTRA */
            0x00000002,   /* FBIO_CFG6 */
            0x00000000,   /* ODT_WRITE */
            0x00000000,   /* ODT_READ */
            0x00000083,   /* FBIO_CFG5 */
            0xE034048B,   /* CFG_DIG_DLL */
            0x007FC010,   /* DLL_XFORM_DQS */
            0x00000000,   /* DLL_XFORM_QUSE */
            0x00000000,   /* ZCAL_REF_CNT */
            0x00000000,   /* ZCAL_WAIT_CNT */
            0x00000000,   /* AUTO_CAL_INTERVAL */
            0x00000000,   /* CFG_CLKTRIM_0 */
            0x00000000,   /* CFG_CLKTRIM_1 */
            0x00000000,   /* CFG_CLKTRIM_2 */
        }
    }
};

#endif

void __init shuttle_init_emc(void)
{
	tegra_init_emc(shuttle_emc_tables, ARRAY_SIZE(shuttle_emc_tables));
}
