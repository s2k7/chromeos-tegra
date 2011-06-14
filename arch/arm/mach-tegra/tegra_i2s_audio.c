/*
 * arch/arm/mach-tegra/tegra_i2s_audio.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *      Iliyan Malchev <malchev@google.com>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/kfifo.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/sysfs.h>
#include <linux/pm_qos_params.h>
#include <linux/delay.h>
#include <linux/tegra_audio.h>
#include <linux/pm.h>
#include <linux/workqueue.h>

#include <mach/dma.h>
#include <mach/iomap.h>
#include <mach/i2s.h>
#include <mach/audio.h>
#include <mach/irqs.h>

#include "clock.h"

#define PCM_BUFFER_MAX_SIZE_ORDER	(PAGE_SHIFT + 2)
#define PCM_BUFFER_DMA_CHUNK_SIZE_ORDER	PAGE_SHIFT
#define PCM_BUFFER_THRESHOLD_ORDER	(PCM_BUFFER_MAX_SIZE_ORDER - 1)
#define PCM_DMA_CHUNK_MIN_SIZE_ORDER	3

#define PCM_IN_BUFFER_PADDING		(1<<6) /* bytes */

#define TEGRA_AUDIO_DSP_NONE		0
#define TEGRA_AUDIO_DSP_PCM		1
#define TEGRA_AUDIO_DSP_NETWORK		2
#define TEGRA_AUDIO_DSP_TDM		3

/* per stream (input/output) */
struct audio_stream {
	int opened;
	struct mutex lock;

	struct tegra_audio_buf_config buf_config;
	bool active; /* is DMA or PIO in progress? */
	void *buffer;
	dma_addr_t buf_phys;
	struct kfifo fifo;
	struct completion fifo_completion;
	struct scatterlist sg;

	struct tegra_audio_error_counts errors;

	int i2s_fifo_atn_level;

	ktime_t last_dma_ts;
	struct tegra_dma_channel *dma_chan;
	bool stop;
	struct completion stop_completion;
	spinlock_t dma_req_lock; /* guards dma_has_it */
	int dma_has_it;
	struct tegra_dma_req dma_req;

	struct pm_qos_request_list pm_qos;
	struct work_struct allow_suspend_work;
};

struct i2s_pio_stats {
	u32 i2s_interrupt_count;
	u32 tx_fifo_errors;
	u32 rx_fifo_errors;
	u32 tx_fifo_written;
	u32 rx_fifo_read;
};

static const int divs_8000[] = { 5, 6, 6, 5 }; /* 8018.(18) Hz */
static const int divs_11025[] = { 4 };
static const int divs_22050[] = { 2 };
static const int divs_44100[] = { 1 };
static const int divs_16000[] = { 2, 3, 3, 3, 3, 3, 3, 2 }; /* 16036.(36) Hz */

/* per i2s controller */
struct audio_driver_state {
	struct list_head next;

	struct platform_device *pdev;
	struct tegra_audio_platform_data *pdata;
	phys_addr_t i2s_phys;
	unsigned long i2s_base;

	bool using_dma;
	unsigned long dma_req_sel;

	int irq; /* for pio mode */
	struct i2s_pio_stats pio_stats;
	struct tegra_audio_in_config in_config;
	const int *in_divs;
	int in_divs_len;

	struct miscdevice misc_out;
	struct miscdevice misc_out_ctl;
	struct audio_stream out;

	struct miscdevice misc_in;
	struct miscdevice misc_in_ctl;
	struct audio_stream in;

	/* Control for whole I2S (Data format, etc.) */
	struct miscdevice misc_ctl;
	unsigned int bit_format;
};

static inline int buf_size(struct audio_stream *s)
{
	return 1 << s->buf_config.size;
}

static inline int chunk_size(struct audio_stream *s)
{
	return 1 << s->buf_config.chunk;
}

static inline int threshold_size(struct audio_stream *s)
{
	return 1 << s->buf_config.threshold;
}

static inline struct audio_driver_state *ads_from_misc_out(struct file *file)
{
	struct miscdevice *m = file->private_data;
	struct audio_driver_state *ads =
			container_of(m, struct audio_driver_state, misc_out);
	BUG_ON(!ads);
	return ads;
}

static inline struct audio_driver_state *ads_from_misc_out_ctl(
		struct file *file)
{
	struct miscdevice *m = file->private_data;
	struct audio_driver_state *ads =
			container_of(m, struct audio_driver_state,
					misc_out_ctl);
	BUG_ON(!ads);
	return ads;
}

static inline struct audio_driver_state *ads_from_misc_in(struct file *file)
{
	struct miscdevice *m = file->private_data;
	struct audio_driver_state *ads =
			container_of(m, struct audio_driver_state, misc_in);
	BUG_ON(!ads);
	return ads;
}

static inline struct audio_driver_state *ads_from_misc_in_ctl(
		struct file *file)
{
	struct miscdevice *m = file->private_data;
	struct audio_driver_state *ads =
			container_of(m, struct audio_driver_state,
					misc_in_ctl);
	BUG_ON(!ads);
	return ads;
}

static inline struct audio_driver_state *ads_from_misc_ctl(
		struct file *file)
{
	struct miscdevice *m = file->private_data;
	struct audio_driver_state *ads =
			container_of(m, struct audio_driver_state,
					misc_ctl);
	BUG_ON(!ads);
	return ads;
}

static inline struct audio_driver_state *ads_from_out(
			struct audio_stream *aos)
{
	return container_of(aos, struct audio_driver_state, out);
}

static inline struct audio_driver_state *ads_from_in(
			struct audio_stream *ais)
{
	return container_of(ais, struct audio_driver_state, in);
}

static inline void prevent_suspend(struct audio_stream *as)
{
	pr_debug("%s\n", __func__);
	cancel_work_sync(&as->allow_suspend_work);
	pm_qos_update_request(&as->pm_qos, 0);
}

static void allow_suspend_worker(struct work_struct *w)
{
	struct audio_stream *as = container_of(w,
			struct audio_stream, allow_suspend_work);

	pr_debug("%s\n", __func__);
	pm_qos_update_request(&as->pm_qos, PM_QOS_DEFAULT_VALUE);
}

static inline void allow_suspend(struct audio_stream *as)
{
	schedule_work(&as->allow_suspend_work);
}

#define I2S_I2S_FIFO_TX_BUSY	I2S_I2S_STATUS_FIFO1_BSY
#define I2S_I2S_FIFO_TX_QS	I2S_I2S_STATUS_QS_FIFO1
#define I2S_I2S_FIFO_TX_ERR	I2S_I2S_STATUS_FIFO1_ERR

#define I2S_I2S_FIFO_RX_BUSY	I2S_I2S_STATUS_FIFO2_BSY
#define I2S_I2S_FIFO_RX_QS	I2S_I2S_STATUS_QS_FIFO2
#define I2S_I2S_FIFO_RX_ERR	I2S_I2S_STATUS_FIFO2_ERR

#define I2S_FIFO_ERR (I2S_I2S_STATUS_FIFO1_ERR | I2S_I2S_STATUS_FIFO2_ERR)

static inline void i2s_writel(unsigned long base, u32 val, u32 reg)
{
	writel(val, base + reg);
}

static inline u32 i2s_readl(unsigned long base, u32 reg)
{
	return readl(base + reg);
}

static inline void i2s_fifo_write(unsigned long base, int fifo, u32 data)
{
	i2s_writel(base, data, fifo ? I2S_I2S_FIFO2_0 : I2S_I2S_FIFO1_0);
}

static inline u32 i2s_fifo_read(unsigned long base, int fifo)
{
	return i2s_readl(base, fifo ? I2S_I2S_FIFO2_0 : I2S_I2S_FIFO1_0);
}

static int i2s_set_channel_bit_count(unsigned long base,
			int sampling, int bitclk)
{
	u32 val;
	int bitcnt = bitclk / (2 * sampling) - 1;

	if (bitcnt < 0 || bitcnt >= 1<<11) {
		pr_err("%s: bit count %d is out of bounds\n", __func__,
			bitcnt);
		return -EINVAL;
	}

	val = bitcnt;
	if (bitclk % (2 * sampling)) {
		pr_info("%s: enabling non-symmetric mode\n", __func__);
		val |= I2S_I2S_TIMING_NON_SYM_ENABLE;
	}

	pr_debug("%s: I2S_I2S_TIMING_0 = %08x\n", __func__, val);
	i2s_writel(base, val, I2S_I2S_TIMING_0);
	return 0;
}

static void i2s_set_fifo_mode(unsigned long base, int fifo, int tx)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	if (fifo == 0) {
		val &= ~I2S_I2S_CTRL_FIFO1_RX_ENABLE;
		val |= (!tx) ? I2S_I2S_CTRL_FIFO1_RX_ENABLE : 0;
	} else {
		val &= ~I2S_I2S_CTRL_FIFO2_TX_ENABLE;
		val |= tx ? I2S_I2S_CTRL_FIFO2_TX_ENABLE : 0;
	}
	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static int i2s_fifo_set_attention_level(unsigned long base,
			int fifo, unsigned level)
{
	u32 val;

	if (level > I2S_FIFO_ATN_LVL_TWELVE_SLOTS) {
		pr_err("%s: invalid fifo level selector %d\n", __func__,
			level);
		return -EINVAL;
	}

	val = i2s_readl(base, I2S_I2S_FIFO_SCR_0);

	if (!fifo) {
		val &= ~I2S_I2S_FIFO_SCR_FIFO1_ATN_LVL_MASK;
		val |= level << I2S_FIFO1_ATN_LVL_SHIFT;
	} else {
		val &= ~I2S_I2S_FIFO_SCR_FIFO2_ATN_LVL_MASK;
		val |= level << I2S_FIFO2_ATN_LVL_SHIFT;
	}

	i2s_writel(base, val, I2S_I2S_FIFO_SCR_0);
	return 0;
}

static void i2s_fifo_enable(unsigned long base, int fifo, int on)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	if (!fifo) {
		val &= ~I2S_I2S_CTRL_FIFO1_ENABLE;
		val |= on ? I2S_I2S_CTRL_FIFO1_ENABLE : 0;
	} else {
		val &= ~I2S_I2S_CTRL_FIFO2_ENABLE;
		val |= on ? I2S_I2S_CTRL_FIFO2_ENABLE : 0;
	}

	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static bool i2s_is_fifo_enabled(unsigned long base, int fifo)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	if (!fifo)
		return !!(val & I2S_I2S_CTRL_FIFO1_ENABLE);
	return !!(val & I2S_I2S_CTRL_FIFO2_ENABLE);
}

static void i2s_fifo_clear(unsigned long base, int fifo)
{
	u32 val = i2s_readl(base, I2S_I2S_FIFO_SCR_0);
	if (!fifo) {
		val &= ~I2S_I2S_FIFO_SCR_FIFO1_CLR;
		val |= I2S_I2S_FIFO_SCR_FIFO1_CLR;
#if 0
		/* Per Nvidia, reduces pop on the next run. */
		if (!(val & I2S_I2S_CTRL_FIFO1_RX_ENABLE)) {
			int cnt = 16;
			while (cnt--)
				i2s_writel(base, 0, I2S_I2S_FIFO1_0);
		}
#endif
	} else {
		val &= ~I2S_I2S_FIFO_SCR_FIFO2_CLR;
		val |= I2S_I2S_FIFO_SCR_FIFO2_CLR;
	}

	i2s_writel(base, val, I2S_I2S_FIFO_SCR_0);
}

static void i2s_set_master(unsigned long base, int master)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_MASTER_ENABLE;
	val |= master ? I2S_I2S_CTRL_MASTER_ENABLE : 0;
	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static int i2s_set_dsp_mode(unsigned long base, unsigned int mode)
{
	u32 val;
	if (mode > TEGRA_AUDIO_DSP_TDM) {
		pr_err("%s: invalid mode %d.\n", __func__, mode);
		return -EINVAL;
	}
	if (mode == TEGRA_AUDIO_DSP_TDM) {
		pr_err("TEGRA_AUDIO_DSP_TDM not implemented.\n");
		return -EINVAL;
	}

	/* Disable unused modes */
	if (mode != TEGRA_AUDIO_DSP_PCM) {
		/* Disable PCM mode */
		val = i2s_readl(base, I2S_I2S_PCM_CTRL_0);
		val &= ~(I2S_I2S_PCM_CTRL_TRM_MODE |
				I2S_I2S_PCM_CTRL_RCV_MODE);
		i2s_writel(base, val, I2S_I2S_PCM_CTRL_0);
	}
	if (mode != TEGRA_AUDIO_DSP_NETWORK) {
		/* Disable Network mode */
		val = i2s_readl(base, I2S_I2S_NW_CTRL_0);
		val &= ~(I2S_I2S_NW_CTRL_TRM_TLPHY_MODE |
				I2S_I2S_NW_CTRL_RCV_TLPHY_MODE);
		i2s_writel(base, val, I2S_I2S_NW_CTRL_0);
	}

	/* Enable the selected mode. */
	switch (mode) {
	case TEGRA_AUDIO_DSP_NETWORK:
		/* Set DSP Network (Telephony) Mode */
		val = i2s_readl(base, I2S_I2S_NW_CTRL_0);
		val |= I2S_I2S_NW_CTRL_TRM_TLPHY_MODE |
				I2S_I2S_NW_CTRL_RCV_TLPHY_MODE;
		i2s_writel(base, val, I2S_I2S_NW_CTRL_0);
		break;
	case TEGRA_AUDIO_DSP_PCM:
		/* Set DSP PCM Mode */
		val = i2s_readl(base, I2S_I2S_PCM_CTRL_0);
		val |= I2S_I2S_PCM_CTRL_TRM_MODE |
				I2S_I2S_PCM_CTRL_RCV_MODE;
		i2s_writel(base, val, I2S_I2S_PCM_CTRL_0);
		break;
	}

	return 0;
}

static int i2s_set_bit_format(unsigned long base, unsigned fmt)
{
	u32 val;

	if (fmt > I2S_BIT_FORMAT_DSP) {
		pr_err("%s: invalid bit-format selector %d\n", __func__, fmt);
		return -EINVAL;
	}

	val = i2s_readl(base, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_BIT_FORMAT_MASK;
	val |= fmt << I2S_BIT_FORMAT_SHIFT;
	i2s_writel(base, val, I2S_I2S_CTRL_0);
	/* For DSP format, select DSP PCM mode. */
	/* PCM mode and Network Mode slot 0 are effectively identical. */
	if (fmt == I2S_BIT_FORMAT_DSP)
		i2s_set_dsp_mode(base, TEGRA_AUDIO_DSP_PCM);
	else
		i2s_set_dsp_mode(base, TEGRA_AUDIO_DSP_NONE);

	return 0;
}

static int i2s_set_bit_size(unsigned long base, unsigned bit_size)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_BIT_SIZE_MASK;

	if (bit_size > I2S_BIT_SIZE_32) {
		pr_err("%s: invalid bit_size selector %d\n", __func__,
			bit_size);
		return -EINVAL;
	}

	val |= bit_size << I2S_BIT_SIZE_SHIFT;

	i2s_writel(base, val, I2S_I2S_CTRL_0);
	return 0;
}

static int i2s_set_fifo_format(unsigned long base, unsigned fmt)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_FIFO_FORMAT_MASK;

	if (fmt > I2S_FIFO_32 && fmt != I2S_FIFO_PACKED) {
		pr_err("%s: invalid fmt selector %d\n", __func__, fmt);
		return -EINVAL;
	}

	val |= fmt << I2S_FIFO_SHIFT;

	i2s_writel(base, val, I2S_I2S_CTRL_0);
	return 0;
}

static void i2s_set_left_right_control_polarity(unsigned long base,
		int high_low)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_L_R_CTRL;
	val |= high_low ? I2S_I2S_CTRL_L_R_CTRL : 0;
	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static void i2s_set_fifo_irq_on_err(unsigned long base, int fifo, int on)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	if (!fifo) {
		val &= ~I2S_I2S_IE_FIFO1_ERR;
		val |= on ? I2S_I2S_IE_FIFO1_ERR : 0;
	} else {
		val &= ~I2S_I2S_IE_FIFO2_ERR;
		val |= on ? I2S_I2S_IE_FIFO2_ERR : 0;
	}
	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static void i2s_set_fifo_irq_on_qe(unsigned long base, int fifo, int on)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	if (!fifo) {
		val &= ~I2S_I2S_QE_FIFO1;
		val |= on ? I2S_I2S_QE_FIFO1 : 0;
	} else {
		val &= ~I2S_I2S_QE_FIFO2;
		val |= on ? I2S_I2S_QE_FIFO2 : 0;
	}
	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static void i2s_enable_fifos(unsigned long base, int on)
{
	u32 val = i2s_readl(base, I2S_I2S_CTRL_0);
	if (on)
		val |= I2S_I2S_QE_FIFO1 | I2S_I2S_QE_FIFO2 |
		       I2S_I2S_IE_FIFO1_ERR | I2S_I2S_IE_FIFO2_ERR;
	else
		val &= ~(I2S_I2S_QE_FIFO1 | I2S_I2S_QE_FIFO2 |
			 I2S_I2S_IE_FIFO1_ERR | I2S_I2S_IE_FIFO2_ERR);

	i2s_writel(base, val, I2S_I2S_CTRL_0);
}

static inline u32 i2s_get_status(unsigned long base)
{
	return i2s_readl(base, I2S_I2S_STATUS_0);
}

static inline u32 i2s_get_control(unsigned long base)
{
	return i2s_readl(base, I2S_I2S_CTRL_0);
}

static inline void i2s_ack_status(unsigned long base)
{
	return i2s_writel(base, i2s_readl(base, I2S_I2S_STATUS_0),
				I2S_I2S_STATUS_0);
}

static inline u32 i2s_get_fifo_scr(unsigned long base)
{
	return i2s_readl(base, I2S_I2S_FIFO_SCR_0);
}

static inline phys_addr_t i2s_get_fifo_phy_base(unsigned long phy_base,
		int fifo)
{
	return phy_base + (fifo ? I2S_I2S_FIFO2_0 : I2S_I2S_FIFO1_0);
}

static inline u32 i2s_get_fifo_full_empty_count(unsigned long base, int fifo)
{
	u32 val = i2s_readl(base, I2S_I2S_FIFO_SCR_0);

	if (!fifo)
		val = val >> I2S_I2S_FIFO_SCR_FIFO1_FULL_EMPTY_COUNT_SHIFT;
	else
		val = val >> I2S_I2S_FIFO_SCR_FIFO2_FULL_EMPTY_COUNT_SHIFT;

	return val & I2S_I2S_FIFO_SCR_FIFO_FULL_EMPTY_COUNT_MASK;
}

static int i2s_configure(struct platform_device *pdev)
{
	struct tegra_audio_platform_data *pdata = pdev->dev.platform_data;
	struct audio_driver_state *state = pdata->driver_data;
	bool master;
	struct clk *i2s_clk;
	int master_clk;

	/* dev_info(&pdev->dev, "%s\n", __func__); */

	if (!state)
		return -ENOMEM;

	/* disable interrupts from I2S */
	i2s_enable_fifos(state->i2s_base, 0);
	i2s_fifo_clear(state->i2s_base, I2S_FIFO_TX);
	i2s_fifo_clear(state->i2s_base, I2S_FIFO_RX);
	i2s_set_left_right_control_polarity(state->i2s_base, 0); /* default */

	i2s_clk = clk_get(&pdev->dev, NULL);
	if (!i2s_clk) {
		dev_err(&pdev->dev, "%s: could not get i2s clock\n",
			__func__);
		return -EIO;
	}

	master = state->bit_format == TEGRA_AUDIO_BIT_FORMAT_DSP ?
			state->pdata->dsp_master : state->pdata->i2s_master;


	master_clk = state->bit_format == TEGRA_AUDIO_BIT_FORMAT_DSP ?
			state->pdata->dsp_master_clk :
			state->pdata->i2s_master_clk;
#define I2S_CLK_FUDGE_FACTOR 2  /* Todo, fix this! */
	if (master)
		i2s_set_channel_bit_count(state->i2s_base, master_clk,
			clk_get_rate(i2s_clk)*I2S_CLK_FUDGE_FACTOR);
	i2s_set_master(state->i2s_base, master);

	i2s_set_fifo_mode(state->i2s_base, I2S_FIFO_TX, 1);
	i2s_set_fifo_mode(state->i2s_base, I2S_FIFO_RX, 0);

	if (state->bit_format == TEGRA_AUDIO_BIT_FORMAT_DSP)
		i2s_set_bit_format(state->i2s_base, I2S_BIT_FORMAT_DSP);
	else
		i2s_set_bit_format(state->i2s_base, state->pdata->mode);
	i2s_set_bit_size(state->i2s_base, state->pdata->bit_size);
	i2s_set_fifo_format(state->i2s_base, state->pdata->fifo_fmt);

	return 0;
}

static int init_stream_buffer(struct audio_stream *,
		struct tegra_audio_buf_config *cfg, unsigned);

static int setup_dma(struct audio_driver_state *);
static void tear_down_dma(struct audio_driver_state *);
static int start_dma_playback(struct audio_stream *);
static void stop_dma_playback(struct audio_stream *);
static int start_dma_recording(struct audio_stream *);
static int resume_dma_recording(struct audio_stream *);
static void stop_dma_recording(struct audio_stream *);

static int setup_pio(struct audio_driver_state *);
static void tear_down_pio(struct audio_driver_state *);
static int start_pio_playback(struct audio_stream *);
static void stop_pio_playback(struct audio_stream *);
static int start_pio_recording(struct audio_stream *);
static void stop_pio_recording(struct audio_stream *);

struct sound_ops {
	int (*setup)(struct audio_driver_state *);
	void (*tear_down)(struct audio_driver_state *);
	int (*start_playback)(struct audio_stream *);
	void (*stop_playback)(struct audio_stream *);
	int (*start_recording)(struct audio_stream *);
	void (*stop_recording)(struct audio_stream *);
};

static const struct sound_ops dma_sound_ops = {
	.setup = setup_dma,
	.tear_down = tear_down_dma,
	.start_playback = start_dma_playback,
	.stop_playback = stop_dma_playback,
	.start_recording = start_dma_recording,
	.stop_recording = stop_dma_recording,
};

static const struct sound_ops pio_sound_ops = {
	.setup = setup_pio,
	.tear_down = tear_down_pio,
	.start_playback = start_pio_playback,
	.stop_playback = stop_pio_playback,
	.start_recording = start_pio_recording,
	.stop_recording = stop_pio_recording,
};

static const struct sound_ops *sound_ops = &dma_sound_ops;

static int start_playback(struct audio_stream *aos)
{
	int rc;
	unsigned long flags;
	spin_lock_irqsave(&aos->dma_req_lock, flags);
	pr_debug("%s: starting playback\n", __func__);
	rc = sound_ops->start_playback(aos);
	spin_unlock_irqrestore(&aos->dma_req_lock, flags);
	if (!rc)
		prevent_suspend(aos);
	return rc;
}

static int start_recording_if_necessary(struct audio_stream *ais)
{
	int rc = 0;
	bool started = false;
	unsigned long flags;

	spin_lock_irqsave(&ais->dma_req_lock, flags);
	if (!ais->stop && !kfifo_is_full(&ais->fifo)) {
		pr_debug("%s: starting recording\n", __func__);
		rc = sound_ops->start_recording(ais);
		started = !rc;
	}
	spin_unlock_irqrestore(&ais->dma_req_lock, flags);
	if (started)
		prevent_suspend(ais);
	return rc;
}

static bool stop_playback_if_necessary(struct audio_stream *aos)
{
	unsigned long flags;
	spin_lock_irqsave(&aos->dma_req_lock, flags);
	if (kfifo_is_empty(&aos->fifo)) {
		sound_ops->stop_playback(aos);
		spin_unlock_irqrestore(&aos->dma_req_lock, flags);
		allow_suspend(aos);
		return true;
	}
	spin_unlock_irqrestore(&aos->dma_req_lock, flags);

	return false;
}

static bool stop_recording_if_necessary_nosync(struct audio_stream *ais)
{
	if (ais->stop || kfifo_is_full(&ais->fifo)) {
		if (kfifo_is_full(&ais->fifo))
			ais->errors.full_empty++;  /* overflow */
		sound_ops->stop_recording(ais);
		return true;
	}

	return false;
}

/* playback and recording */
static bool wait_till_stopped(struct audio_stream *as)
{
	int rc;
	pr_debug("%s: wait for completion\n", __func__);
	rc = wait_for_completion_interruptible_timeout(
			&as->stop_completion, HZ);
	if (!rc)
		pr_err("%s: wait timed out", __func__);
	allow_suspend(as);
	pr_debug("%s: done: %d\n", __func__, rc);
	return true;
}

/* Ask for playback and recording to stop.  The _nosync means that
 * as->lock has to be locked by the caller.
 */
static void request_stop_nosync(struct audio_stream *as)
{
	pr_debug("%s\n", __func__);
	if (!as->stop) {
		as->stop = true;
		wait_till_stopped(as);
		if (!completion_done(&as->fifo_completion)) {
			pr_debug("%s: complete\n", __func__);
			complete(&as->fifo_completion);
		}
	}
	kfifo_reset(&as->fifo);
	as->active = false; /* applies to recording only */
	pr_debug("%s: done\n", __func__);
}

static void toggle_dma(struct audio_driver_state *ads)
{
	pr_info("%s: %s\n", __func__, ads->using_dma ? "pio" : "dma");
	sound_ops->tear_down(ads);
	sound_ops = ads->using_dma ? &pio_sound_ops : &dma_sound_ops;
	sound_ops->setup(ads);
	ads->using_dma = !ads->using_dma;
}

/* DMA */

static int resume_dma_playback(struct audio_stream *aos);

static void setup_dma_tx_request(struct tegra_dma_req *req,
		struct audio_stream *aos);

static void setup_dma_rx_request(struct tegra_dma_req *req,
		struct audio_stream *ais);

static int setup_dma(struct audio_driver_state *ads)
{
	int rc;
	pr_info("%s\n", __func__);

	if ((ads->pdata->mask & TEGRA_AUDIO_ENABLE_TX)) {
		/* setup audio playback */
		ads->out.buf_phys = dma_map_single(&ads->pdev->dev,
					ads->out.buffer,
					1 << PCM_BUFFER_MAX_SIZE_ORDER,
					DMA_TO_DEVICE);
		BUG_ON(!ads->out.buf_phys);
		setup_dma_tx_request(&ads->out.dma_req, &ads->out);
		ads->out.dma_chan =
			tegra_dma_allocate_channel(TEGRA_DMA_MODE_ONESHOT);
		if (!ads->out.dma_chan) {
			pr_err("%s: error allocating output DMA channel: %ld\n",
				__func__, PTR_ERR(ads->out.dma_chan));
			rc = -ENODEV;
			goto fail_tx;
		}
	}

	if ((ads->pdata->mask & TEGRA_AUDIO_ENABLE_RX)) {
		/* setup audio recording */
		ads->in.buf_phys = dma_map_single(&ads->pdev->dev,
					ads->in.buffer,
					1 << PCM_BUFFER_MAX_SIZE_ORDER,
					DMA_FROM_DEVICE);
		BUG_ON(!ads->in.buf_phys);
		setup_dma_rx_request(&ads->in.dma_req, &ads->in);
		ads->in.dma_chan =
			tegra_dma_allocate_channel(TEGRA_DMA_MODE_ONESHOT);
		if (!ads->in.dma_chan) {
			pr_err("%s: error allocating input DMA channel: %ld\n",
				__func__, PTR_ERR(ads->in.dma_chan));
			rc = -ENODEV;
			goto fail_rx;
		}
	}

	return 0;

fail_rx:
	if ((ads->pdata->mask & TEGRA_AUDIO_ENABLE_RX)) {
		dma_unmap_single(&ads->pdev->dev, ads->in.buf_phys,
			1 << PCM_BUFFER_MAX_SIZE_ORDER, DMA_FROM_DEVICE);
		tegra_dma_free_channel(ads->in.dma_chan);
		ads->in.dma_chan = 0;
	}
fail_tx:
	if ((ads->pdata->mask & TEGRA_AUDIO_ENABLE_TX)) {
		dma_unmap_single(&ads->pdev->dev, ads->out.buf_phys,
			1 << PCM_BUFFER_MAX_SIZE_ORDER, DMA_TO_DEVICE);
		tegra_dma_free_channel(ads->out.dma_chan);
		ads->out.dma_chan = 0;
	}

	return rc;
}

static void tear_down_dma(struct audio_driver_state *ads)
{
	pr_info("%s\n", __func__);

	tegra_dma_free_channel(ads->out.dma_chan);
	ads->out.dma_chan = NULL;
	dma_unmap_single(&ads->pdev->dev, ads->out.buf_phys,
				buf_size(&ads->out),
				DMA_TO_DEVICE);
	ads->out.buf_phys = 0;

	tegra_dma_free_channel(ads->in.dma_chan);
	ads->in.dma_chan = NULL;
	dma_unmap_single(&ads->pdev->dev, ads->in.buf_phys,
				buf_size(&ads->in),
				DMA_FROM_DEVICE);
	ads->in.buf_phys = 0;
}

static void dma_tx_complete_callback(struct tegra_dma_req *req)
{
	unsigned long flags;
	struct audio_stream *aos = req->dev;
	int count = req->bytes_transferred;
	u64 delta_us;
	u64 max_delay_us = count * 10000 / (4 * 441);

	pr_debug("%s bytes transferred %d\n", __func__, count);

	aos->dma_has_it = false;
	delta_us = ktime_to_us(ktime_sub(ktime_get_real(), aos->last_dma_ts));

	if (delta_us > max_delay_us) {
		pr_debug("%s: too late by %lld us\n", __func__,
			delta_us - max_delay_us);
		aos->errors.late_dma++;
	}

	kfifo_dma_out_finish(&aos->fifo, count);
	dma_unmap_sg(NULL, &aos->sg, 1, DMA_TO_DEVICE);

	if (!completion_done(&aos->fifo_completion)) {
		pr_debug("%s: complete (%d avail)\n", __func__,
				kfifo_avail(&aos->fifo));
		complete(&aos->fifo_completion);
	}

	if (stop_playback_if_necessary(aos)) {
		pr_debug("%s: done (stopped)\n", __func__);
		if (!completion_done(&aos->stop_completion)) {
			pr_debug("%s: signalling stop completion\n", __func__);
			complete(&aos->stop_completion);
		}
		return;
	}

	spin_lock_irqsave(&aos->dma_req_lock, flags);
	resume_dma_playback(aos);
	spin_unlock_irqrestore(&aos->dma_req_lock, flags);
}

static void dma_rx_complete_threshold(struct tegra_dma_req *req)
{
	pr_debug("%s\n", __func__);
}

static void dma_rx_complete_callback(struct tegra_dma_req *req)
{
	unsigned long flags;
	struct audio_stream *ais = req->dev;
	int count = req->bytes_transferred;

	spin_lock_irqsave(&ais->dma_req_lock, flags);

	ais->dma_has_it = false;

	pr_debug("%s(%d): transferred %d bytes (%d available in fifo)\n",
			__func__,
			smp_processor_id(),
			count, kfifo_avail(&ais->fifo));

	BUG_ON(kfifo_avail(&ais->fifo) < count);
	kfifo_dma_in_finish(&ais->fifo, count);
	dma_unmap_sg(NULL, &ais->sg, 1, DMA_FROM_DEVICE);

	if (!completion_done(&ais->fifo_completion)) {
		pr_debug("%s: signalling fifo completion\n", __func__);
		complete(&ais->fifo_completion);
	}

	if (stop_recording_if_necessary_nosync(ais)) {
		spin_unlock_irqrestore(&ais->dma_req_lock, flags);
		pr_debug("%s: done (stopped)\n", __func__);
		if (!completion_done(&ais->stop_completion)) {
			pr_debug("%s: signalling stop completion\n", __func__);
			complete(&ais->stop_completion);
		}
		return;
	}

	pr_debug("%s: resuming dma recording\n", __func__);

	/* This call will fail if we try to set up a DMA request that's
	 * too small.
	 */
	(void)resume_dma_recording(ais);
	spin_unlock_irqrestore(&ais->dma_req_lock, flags);

	pr_debug("%s: done\n", __func__);
}

static void setup_dma_tx_request(struct tegra_dma_req *req,
		struct audio_stream *aos)
{
	struct audio_driver_state *ads = ads_from_out(aos);

	memset(req, 0, sizeof(*req));

	req->complete = dma_tx_complete_callback;
	req->dev = aos;
	req->to_memory = false;
	req->dest_addr = i2s_get_fifo_phy_base(ads->i2s_phys, I2S_FIFO_TX);
	req->dest_wrap = 4;
	if (ads->bit_format == TEGRA_AUDIO_BIT_FORMAT_DSP)
		req->dest_bus_width = ads->pdata->dsp_bus_width;
	else
		req->dest_bus_width = ads->pdata->i2s_bus_width;
	req->source_bus_width = 32;
	req->source_wrap = 0;
	req->req_sel = ads->dma_req_sel;
}

static void setup_dma_rx_request(struct tegra_dma_req *req,
		struct audio_stream *ais)
{
	struct audio_driver_state *ads = ads_from_in(ais);

	memset(req, 0, sizeof(*req));

	req->complete = dma_rx_complete_callback;
	req->threshold = dma_rx_complete_threshold;
	req->dev = ais;
	req->to_memory = true;
	req->source_addr = i2s_get_fifo_phy_base(ads->i2s_phys, I2S_FIFO_RX);
	req->source_wrap = 4;
	if (ads->bit_format == TEGRA_AUDIO_BIT_FORMAT_DSP)
		req->source_bus_width = ads->pdata->dsp_bus_width;
	else
		req->source_bus_width = ads->pdata->i2s_bus_width;
	req->dest_bus_width = 32;
	req->dest_wrap = 0;
	req->req_sel = ads->dma_req_sel;
}

/* Called with aos->dma_req_lock taken. */
static int resume_dma_playback(struct audio_stream *aos)
{
	int rc;
	struct audio_driver_state *ads = ads_from_out(aos);
	struct tegra_dma_req *req = &aos->dma_req;

	if (aos->dma_has_it) {
		pr_debug("%s: playback already in progress\n", __func__);
		return -EALREADY;
	}

	rc = kfifo_dma_out_prepare(&aos->fifo, &aos->sg,
			1, kfifo_len(&aos->fifo));
	/* stop_playback_if_necessary() already checks to see if the fifo is
	 * empty.
	 */
	BUG_ON(!rc);
	rc = dma_map_sg(NULL, &aos->sg, 1, DMA_TO_DEVICE);
	if (rc < 0) {
		pr_err("%s: could not map dma memory: %d\n", __func__, rc);
		return rc;
	}

#if 0
	i2s_fifo_clear(ads->i2s_base, I2S_FIFO_TX);
#endif
	i2s_fifo_set_attention_level(ads->i2s_base,
			I2S_FIFO_TX, aos->i2s_fifo_atn_level);


	req->source_addr = sg_dma_address(&aos->sg);
	req->size = sg_dma_len(&aos->sg);
	dma_sync_single_for_device(NULL,
			req->source_addr, req->size, DMA_TO_DEVICE);

	/* Don't send all the data yet. */
	if (req->size > chunk_size(aos))
		req->size = chunk_size(aos);
	pr_debug("%s resume playback (%d in fifo, writing %d)\n",
			__func__, kfifo_len(&aos->fifo), req->size);

	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_TX, 1);

	aos->last_dma_ts = ktime_get_real();
	rc = tegra_dma_enqueue_req(aos->dma_chan, req);
	aos->dma_has_it = !rc;
	if (!aos->dma_has_it)
		pr_err("%s: could not enqueue TX DMA req\n", __func__);
	return rc;
}

/* Called with aos->dma_req_lock taken. */
static int start_dma_playback(struct audio_stream *aos)
{
	return resume_dma_playback(aos);
}

/* Called with aos->dma_req_lock taken. */
static void stop_dma_playback(struct audio_stream *aos)
{
	int spin = 0;
	struct audio_driver_state *ads = ads_from_out(aos);
	pr_debug("%s\n", __func__);
	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_TX, 0);
	while ((i2s_get_status(ads->i2s_base) & I2S_I2S_FIFO_TX_BUSY) &&
			spin < 100) {
		udelay(10);
		if (spin++ > 50)
			pr_info("%s: spin %d\n", __func__, spin);
	}
	if (spin == 100)
		pr_warn("%s: spinny\n", __func__);
}

/* This function may be called from either interrupt or process context. */
/* Called with ais->dma_req_lock taken. */
static int resume_dma_recording(struct audio_stream *ais)
{
	int rc;
	struct audio_driver_state *ads = ads_from_in(ais);
	struct tegra_dma_req *req = &ais->dma_req;

	BUG_ON(kfifo_is_full(&ais->fifo));

	if (ais->dma_has_it) {
		pr_debug("%s: recording already in progress\n", __func__);
		return -EALREADY;
	}

	/* Don't send all the data yet. */
	if (req->size > chunk_size(ais))
		req->size = chunk_size(ais);
	rc = kfifo_dma_in_prepare(&ais->fifo, &ais->sg, 1,
			kfifo_avail(&ais->fifo));
	BUG_ON(!rc);
	rc = dma_map_sg(NULL, &ais->sg, 1, DMA_FROM_DEVICE);
	if (rc < 0) {
		pr_err("%s: coult not map dma for recording: %d\n",
				__func__, rc);
		return rc;
	}

	req->dest_addr = sg_dma_address(&ais->sg);
	req->size = round_down(sg_dma_len(&ais->sg), 4);

	if (!req->size) {
		pr_err("%s: invalid request size %d\n", __func__, req->size);
		return -EIO;
	}

	dma_sync_single_for_device(NULL,
			req->dest_addr, req->size, DMA_FROM_DEVICE);

	ais->dma_has_it = !tegra_dma_enqueue_req(ais->dma_chan, &ais->dma_req);
	if (!ais->dma_has_it) {
		pr_err("%s: could not enqueue RX DMA req\n", __func__);
		return -EINVAL;
	}

#if 0
	i2s_fifo_clear(ads->i2s_base, I2S_FIFO_RX);
#endif
	i2s_fifo_set_attention_level(ads->i2s_base,
			I2S_FIFO_RX, ais->i2s_fifo_atn_level);
	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_RX, 1);
	return 0;
}

/* Called with ais->dma_req_lock taken. */
static int start_dma_recording(struct audio_stream *ais)
{
	pr_debug("%s\n", __func__);
	return resume_dma_recording(ais);
}

/* Called with ais->dma_req_lock taken. */
static void stop_dma_recording(struct audio_stream *ais)
{
	int spin = 0;
	struct audio_driver_state *ads = ads_from_in(ais);
	pr_debug("%s\n", __func__);
	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_RX, 0);
	i2s_fifo_clear(ads->i2s_base, I2S_FIFO_RX);
	while ((i2s_get_status(ads->i2s_base) & I2S_I2S_FIFO_RX_BUSY) &&
			spin < 100) {
		udelay(10);
		if (spin++ > 50)
			pr_info("%s: spin %d\n", __func__, spin);
	}
	if (spin == 100)
		pr_warn("%s: spinny\n", __func__);
}

/* PIO (non-DMA) */

static int setup_pio(struct audio_driver_state *ads)
{
	pr_info("%s\n", __func__);
	enable_irq(ads->irq);
	return 0;
}

static void tear_down_pio(struct audio_driver_state *ads)
{
	pr_info("%s\n", __func__);
	disable_irq(ads->irq);
}

static int start_pio_playback(struct audio_stream *aos)
{
	struct audio_driver_state *ads = ads_from_out(aos);

	if (i2s_is_fifo_enabled(ads->i2s_base, I2S_FIFO_TX)) {
		pr_debug("%s: playback is already in progress\n", __func__);
		return -EALREADY;
	}

	pr_debug("%s\n", __func__);

	i2s_fifo_set_attention_level(ads->i2s_base,
			I2S_FIFO_TX, aos->i2s_fifo_atn_level);
#if 0
	i2s_fifo_clear(ads->i2s_base, I2S_FIFO_TX);
#endif

	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_TX, 1);

	i2s_set_fifo_irq_on_err(ads->i2s_base, I2S_FIFO_TX, 1);
	i2s_set_fifo_irq_on_qe(ads->i2s_base, I2S_FIFO_TX, 1);

	return 0;
}

static void stop_pio_playback(struct audio_stream *aos)
{
	struct audio_driver_state *ads = ads_from_out(aos);

	i2s_set_fifo_irq_on_err(ads->i2s_base, I2S_FIFO_TX, 0);
	i2s_set_fifo_irq_on_qe(ads->i2s_base, I2S_FIFO_TX, 0);
	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_TX, 0);
	while (i2s_get_status(ads->i2s_base) & I2S_I2S_FIFO_TX_BUSY)
		/* spin */;

	pr_debug("%s: interrupts %d\n", __func__,
			ads->pio_stats.i2s_interrupt_count);
	pr_info("%s: sent       %d\n", __func__,
			ads->pio_stats.tx_fifo_written);
	pr_info("%s: tx errors  %d\n", __func__,
			ads->pio_stats.tx_fifo_errors);

	memset(&ads->pio_stats, 0, sizeof(ads->pio_stats));
}

static int start_pio_recording(struct audio_stream *ais)
{
	struct audio_driver_state *ads = ads_from_in(ais);

	if (i2s_is_fifo_enabled(ads->i2s_base, I2S_FIFO_RX)) {
		pr_debug("%s: already started\n", __func__);
		return -EALREADY;
	}

	pr_debug("%s: start\n", __func__);

	i2s_fifo_set_attention_level(ads->i2s_base,
			I2S_FIFO_RX, ais->i2s_fifo_atn_level);
#if 0
	i2s_fifo_clear(ads->i2s_base, I2S_FIFO_RX);
#endif

	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_RX, 1);

	i2s_set_fifo_irq_on_err(ads->i2s_base, I2S_FIFO_RX, 1);
	i2s_set_fifo_irq_on_qe(ads->i2s_base, I2S_FIFO_RX, 1);

	return 0;
}

static void stop_pio_recording(struct audio_stream *ais)
{
	struct audio_driver_state *ads = ads_from_in(ais);

	pr_debug("%s\n", __func__);

	i2s_set_fifo_irq_on_err(ads->i2s_base, I2S_FIFO_RX, 0);
	i2s_set_fifo_irq_on_qe(ads->i2s_base, I2S_FIFO_RX, 0);
	i2s_fifo_enable(ads->i2s_base, I2S_FIFO_RX, 0);
	i2s_fifo_clear(ads->i2s_base, I2S_FIFO_RX);

	pr_debug("%s: interrupts %d\n", __func__,
			ads->pio_stats.i2s_interrupt_count);
	pr_debug("%s: received   %d\n", __func__,
			ads->pio_stats.rx_fifo_read);
	pr_debug("%s: rx errors  %d\n", __func__,
			ads->pio_stats.rx_fifo_errors);

	memset(&ads->pio_stats, 0, sizeof(ads->pio_stats));
}

static irqreturn_t i2s_interrupt(int irq, void *data)
{
	struct audio_driver_state *ads = data;
	u32 status = i2s_get_status(ads->i2s_base);

	pr_debug("%s: %08x\n", __func__, status);

	ads->pio_stats.i2s_interrupt_count++;

	if (status & I2S_I2S_FIFO_TX_ERR)
		ads->pio_stats.tx_fifo_errors++;

	if (status & I2S_I2S_FIFO_RX_ERR) {
		ads->pio_stats.rx_fifo_errors++;
		ads->in.errors.full_empty++;
	}

	if (status & I2S_FIFO_ERR)
		i2s_ack_status(ads->i2s_base);

	if (status & I2S_I2S_FIFO_TX_QS) {
		int written;
		int empty;
		int len;
		u16 fifo_buffer[32];

		struct audio_stream *out = &ads->out;

		if (!i2s_is_fifo_enabled(ads->i2s_base, I2S_FIFO_TX)) {
			pr_debug("%s: tx fifo not enabled, skipping\n",
				__func__);
			goto check_rx;
		}

		pr_debug("%s tx fifo is ready\n", __func__);

		if (!completion_done(&out->fifo_completion)) {
			pr_debug("%s: tx complete (%d avail)\n", __func__,
					kfifo_avail(&out->fifo));
			complete(&out->fifo_completion);
		}

		if (stop_playback_if_necessary(out)) {
			pr_debug("%s: done (stopped)\n", __func__);
			if (!completion_done(&out->stop_completion)) {
				pr_debug("%s: signalling stop completion\n",
					__func__);
				complete(&out->stop_completion);
			}
			goto check_rx;
		}

		empty = i2s_get_fifo_full_empty_count(ads->i2s_base,
				I2S_FIFO_TX);

		len = kfifo_out(&out->fifo, fifo_buffer,
				empty * sizeof(u16));
		len /= sizeof(u16);

		written = 0;
		while (empty-- && written < len) {
			ads->pio_stats.tx_fifo_written += written * sizeof(u16);
			i2s_fifo_write(ads->i2s_base,
					I2S_FIFO_TX, fifo_buffer[written++]);
		}

		/* TODO: Should we check to see if we wrote less than the
		 * FIFO threshold and adjust it if so?
		 */

		if (written) {
			/* start the transaction */
			pr_debug("%s: enabling fifo (%d samples written)\n",
					__func__, written);
			i2s_fifo_enable(ads->i2s_base, I2S_FIFO_TX, 1);
		}
	}

check_rx:
	if (status & I2S_I2S_FIFO_RX_QS) {
		int nr;
		int full;

		struct audio_stream *in = &ads->in;

		if (!i2s_is_fifo_enabled(ads->i2s_base, I2S_FIFO_RX)) {
			pr_debug("%s: rx fifo not enabled, skipping\n",
				__func__);
			goto done;
		}

		i2s_fifo_enable(ads->i2s_base, I2S_FIFO_RX, 0);

		full = i2s_get_fifo_full_empty_count(ads->i2s_base,
				I2S_FIFO_RX);

		pr_debug("%s rx fifo is ready (%d samples)\n", __func__, full);

		nr = full;
		while (nr--) {
			u16 sample = i2s_fifo_read(ads->i2s_base, I2S_FIFO_RX);
			kfifo_in(&in->fifo, &sample, sizeof(sample));
		}

		ads->pio_stats.rx_fifo_read += full * sizeof(u16);

		if (!completion_done(&in->fifo_completion)) {
			pr_debug("%s: rx complete (%d avail)\n", __func__,
					kfifo_avail(&in->fifo));
			complete(&in->fifo_completion);
		}

		if (stop_recording_if_necessary_nosync(&ads->in)) {
			pr_debug("%s: recording cancelled or fifo full\n",
				__func__);
			if (!completion_done(&ads->in.stop_completion)) {
				pr_debug("%s: signalling stop completion\n",
					__func__);
				complete(&ads->in.stop_completion);
			}
			goto done;
		}

		i2s_fifo_enable(ads->i2s_base, I2S_FIFO_RX, 1);
	}

done:
	pr_debug("%s: done %08x\n", __func__, i2s_get_status(ads->i2s_base));
	return IRQ_HANDLED;
}

static ssize_t tegra_audio_write(struct file *file,
		const char __user *buf, size_t size, loff_t *off)
{
	ssize_t rc = 0, total = 0;
	unsigned nw = 0;

	struct audio_driver_state *ads = ads_from_misc_out(file);

	mutex_lock(&ads->out.lock);

	if (!IS_ALIGNED(size, 4)) {
		pr_err("%s: user size request %d not aligned to 4\n",
			__func__, size);
		rc = -EINVAL;
		goto done;
	}

	pr_debug("%s: write %d bytes, %d available\n", __func__,
			size, kfifo_avail(&ads->out.fifo));

again:
	if (ads->out.stop) {
		pr_info("%s: playback has been cancelled (%d/%d bytes)\n",
				__func__, total, size);
		goto done;
	}

	rc = kfifo_from_user(&ads->out.fifo, buf + total, size - total, &nw);
	if (rc < 0) {
		pr_err("%s: error copying from user\n", __func__);
		goto done;
	}

	rc = start_playback(&ads->out);
	if (rc < 0 && rc != -EALREADY) {
		pr_err("%s: could not start playback: %d\n", __func__, rc);
		goto done;
	}

	total += nw;
	if (total < size) {
		pr_debug("%s: sleep (user %d total %d nw %d)\n", __func__,
				size, total, nw);
		mutex_unlock(&ads->out.lock);
		rc = wait_for_completion_interruptible(
				&ads->out.fifo_completion);
		mutex_lock(&ads->out.lock);
		if (rc == -ERESTARTSYS) {
			pr_warn("%s: interrupted\n", __func__);
			goto done;
		}
		pr_debug("%s: awake\n", __func__);
		goto again;
	}

	rc = total;
	*off += total;

done:
	mutex_unlock(&ads->out.lock);
	return rc;
}

static long tegra_audio_out_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct audio_driver_state *ads = ads_from_misc_out_ctl(file);
	struct audio_stream *aos = &ads->out;

	mutex_lock(&aos->lock);

	switch (cmd) {
	case TEGRA_AUDIO_OUT_SET_BUF_CONFIG: {
		struct tegra_audio_buf_config cfg;
		if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		if (kfifo_len(&aos->fifo)) {
			pr_err("%s: playback in progress\n", __func__);
			rc = -EBUSY;
			break;
		}
		rc = init_stream_buffer(aos, &cfg, 0);
		if (rc < 0)
			break;
		aos->buf_config = cfg;
	}
		break;
	case TEGRA_AUDIO_OUT_GET_BUF_CONFIG:
		if (copy_to_user((void __user *)arg, &aos->buf_config,
				sizeof(aos->buf_config)))
			rc = -EFAULT;
		break;
	case TEGRA_AUDIO_OUT_GET_ERROR_COUNT:
		if (copy_to_user((void __user *)arg, &aos->errors,
				sizeof(aos->errors)))
			rc = -EFAULT;
		if (!rc)
			memset(&aos->errors, 0, sizeof(aos->errors));
		break;
	case TEGRA_AUDIO_OUT_FLUSH:
		if (kfifo_len(&aos->fifo)) {
			pr_debug("%s: flushing\n", __func__);
			request_stop_nosync(aos);
			pr_debug("%s: flushed\n", __func__);
		}
		aos->stop = false;
		break;
	default:
		rc = -EINVAL;
	}

	mutex_unlock(&aos->lock);
	return rc;
}

static long tegra_audio_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct audio_driver_state *ads = ads_from_misc_ctl(file);
	unsigned int mode;
	bool dma_restart = false;

	mutex_lock(&ads->out.lock);
	mutex_lock(&ads->in.lock);

	switch (cmd) {
	case TEGRA_AUDIO_SET_BIT_FORMAT:
		if (copy_from_user(&mode, (const void __user *)arg,
					sizeof(mode))) {
			rc = -EFAULT;
			goto done;
		}
		dma_restart = (mode != ads->bit_format);
		switch (mode) {
		case TEGRA_AUDIO_BIT_FORMAT_DEFAULT:
			i2s_set_bit_format(ads->i2s_base, ads->pdata->mode);
			ads->bit_format = mode;
			break;
		case TEGRA_AUDIO_BIT_FORMAT_DSP:
			i2s_set_bit_format(ads->i2s_base, I2S_BIT_FORMAT_DSP);
			ads->bit_format = mode;
			break;
		default:
			pr_err("%s: Invald PCM mode %d", __func__, mode);
			rc = -EINVAL;
			goto done;
		}
		break;
	case TEGRA_AUDIO_GET_BIT_FORMAT:
		if (copy_to_user((void __user *)arg, &ads->bit_format,
				sizeof(mode)))
			rc = -EFAULT;
		goto done;
	}

	if (dma_restart && ads->using_dma) {
		pr_debug("%s: Restarting DMA due to configuration change.\n",
			__func__);
		if (kfifo_len(&ads->out.fifo) || ads->in.active) {
			pr_err("%s: dma busy, cannot restart.\n", __func__);
			rc = -EBUSY;
			goto done;
		}
		sound_ops->tear_down(ads);
		i2s_configure(ads->pdev);
		sound_ops->setup(ads);
	}

done:
	mutex_unlock(&ads->in.lock);
	mutex_unlock(&ads->out.lock);
	return rc;
}

static long tegra_audio_in_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct audio_driver_state *ads = ads_from_misc_in_ctl(file);
	struct audio_stream *ais = &ads->in;

	mutex_lock(&ais->lock);

	switch (cmd) {
	case TEGRA_AUDIO_IN_START:
		pr_debug("%s: start recording\n", __func__);
		ais->stop = false;
		rc = start_recording_if_necessary(ais);
		ais->active = !rc || rc == -EALREADY;
		break;
	case TEGRA_AUDIO_IN_STOP:
		pr_debug("%s: start recording\n", __func__);
		if (ais->active)
			request_stop_nosync(ais);
		break;
	case TEGRA_AUDIO_IN_SET_CONFIG: {
		struct tegra_audio_in_config cfg;

		if (ais->active) {
			pr_err("%s: recording in progress\n", __func__);
			rc = -EBUSY;
			break;
		}
		if (copy_from_user(&cfg, (const void __user *)arg,
					sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}

#ifdef SAMPLE_RATE_CONVERTER_IN_DRIVER
		switch (cfg.rate) {
		case 8000:
			ads->in_divs = divs_8000;
			ads->in_divs_len = ARRAY_SIZE(divs_8000);
			break;
		case 11025:
			ads->in_divs = divs_11025;
			ads->in_divs_len = ARRAY_SIZE(divs_11025);
			break;
		case 16000:
			ads->in_divs = divs_16000;
			ads->in_divs_len = ARRAY_SIZE(divs_16000);
			break;
		case 22050:
			ads->in_divs = divs_22050;
			ads->in_divs_len = ARRAY_SIZE(divs_22050);
			break;
		case 44100:
			ads->in_divs = divs_44100;
			ads->in_divs_len = ARRAY_SIZE(divs_44100);
			break;
		default:
			pr_err("%s: invalid sampling rate %d\n", __func__,
				cfg.rate);
			rc = -EINVAL;
			break;
		}
#endif
		if (cfg.stereo && !ads->pdata->stereo_capture) {
			pr_err("%s: not capable of stereo capture.",
				__func__);
			rc = -EINVAL;
		}
		if (!rc) {
			pr_info("%s: setting input sampling rate to %d, %s\n",
				__func__, cfg.rate,
				cfg.stereo ? "stereo" : "mono");
			ads->in_config = cfg;
			ads->in_config.stereo = !!ads->in_config.stereo;
		}
	}
		break;
	case TEGRA_AUDIO_IN_GET_CONFIG:
		if (copy_to_user((void __user *)arg, &ads->in_config,
				sizeof(ads->in_config)))
			rc = -EFAULT;
		break;
	case TEGRA_AUDIO_IN_SET_BUF_CONFIG: {
		struct tegra_audio_buf_config cfg;
		if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		if (ais->active) {
			pr_err("%s: recording in progress\n", __func__);
			rc = -EBUSY;
			break;
		}
		rc = init_stream_buffer(ais, &cfg, PCM_IN_BUFFER_PADDING);
		if (rc < 0)
			break;
		ais->buf_config = cfg;
	}
		break;
	case TEGRA_AUDIO_IN_GET_BUF_CONFIG:
		if (copy_to_user((void __user *)arg, &ais->buf_config,
				sizeof(ais->buf_config)))
			rc = -EFAULT;
		break;
	case TEGRA_AUDIO_IN_GET_ERROR_COUNT:
		if (copy_to_user((void __user *)arg, &ais->errors,
				sizeof(ais->errors)))
			rc = -EFAULT;
		if (!rc)
			memset(&ais->errors, 0, sizeof(ais->errors));
		break;
	default:
		rc = -EINVAL;
	}

	mutex_unlock(&ais->lock);
	return rc;
}

static ssize_t __i2s_copy_to_user(struct audio_driver_state *ads,
				void __user *dst, int dst_size,
				void *src, int src_size,
				int *num_consumed)
{
	int bytes_written = dst_size < src_size ? dst_size : src_size;
	*num_consumed = bytes_written;
	if (copy_to_user(dst, src, bytes_written)) {
		pr_err("%s: error copying %d bytes to user\n", __func__,
			bytes_written);
		return -EFAULT;
	}
	return bytes_written;
}

#ifdef SAMPLE_RATE_CONVERTER_IN_DRIVER

/* downsample a 16-bit 44.1kHz PCM stereo stream to stereo or mono 16-bit PCM
 * stream.
 */

static int downsample(const s16 *in, int in_len,
		s16 *out, int out_len,
		int *consumed, /* from input */
		const int *divs, int divs_len,
		bool out_stereo)
{
	/* Todo: Handle mono source streams */
	int i, j;
	int lsum, rsum;
	int di, div;
	int oi;

	i = 0;
	oi = 0;
	di = 0;
	div = divs[0];
	while (i + div * 2 <= in_len && oi + out_stereo < out_len) {
		for (j = 0, lsum = 0, rsum = 0; j < div; j++) {
			lsum += in[i + j * 2];
			rsum += in[i + j * 2 + 1];
		}
		if (!out_stereo)
			out[oi] = (lsum + rsum) / (div * 2);
		else {
			out[oi] = lsum / div;
			out[oi + 1] = rsum / div;
		}

		oi += out_stereo + 1;
		i += div * 2;
		div = divs[++di % divs_len];
	}

	*consumed = i;

	pr_debug("%s: in_len %d out_len %d consumed %d generated %d\n",
		__func__, in_len, out_len, *consumed, oi);
	return oi;
}

static ssize_t __downsample_to_user(struct audio_driver_state *ads,
				void __user *dst, int dst_size,
				void *src, int src_size,
				int *num_consumed)
{
	int bytes_ds;

	pr_debug("%s\n", __func__);

	bytes_ds = downsample(src, src_size / sizeof(s16),
			src, dst_size / sizeof(s16),
			num_consumed,
			ads->in_divs, ads->in_divs_len,
			ads->in_config.stereo) * sizeof(s16);

	if (copy_to_user(dst, src, bytes_ds)) {
		pr_err("%s: error copying %d bytes to user\n", __func__,
			bytes_ds);
		return -EFAULT;
	}

	*num_consumed *= sizeof(s16);
	BUG_ON(*num_consumed > src_size);

	pr_debug("%s: generated %d, skipped %d, original in fifo %d\n",
			__func__, bytes_ds, *num_consumed, src_size);

	return bytes_ds;
}
#endif /*SAMPLE_RATE_CONVERTER_IN_DRIVER*/

static ssize_t downsample_to_user(struct audio_driver_state *ads,
			void __user *buf,
			size_t size) /* bytes to write to user buffer */
{
	int i, nr_sg;
	int bytes_consumed_from_fifo, bc_now;
	int bytes_ds, ds_now;
	bool take_two = false;

	struct scatterlist sgl[PCM_BUFFER_MAX_SIZE_ORDER - PAGE_SHIFT];
	sg_init_table(sgl, ARRAY_SIZE(sgl));

	if (size == 0) {
		pr_debug("%s: user buffer is full\n", __func__);
		return 0;
	}

	if (kfifo_is_empty(&ads->in.fifo)) {
		pr_debug("%s: input fifo is empty\n", __func__);
		return 0;
	}

	nr_sg = kfifo_dma_out_prepare(&ads->in.fifo,
				sgl, ARRAY_SIZE(sgl),
				kfifo_len(&ads->in.fifo));
	BUG_ON(!nr_sg);

	pr_debug("%s (fifo size %d)\n", __func__, size);

	bytes_ds = 0;
	bytes_consumed_from_fifo = 0;
	for (bytes_ds = 0, i = 0; i < nr_sg; i++) {
		BUG_ON(!sgl[i].length);

again:
#ifdef SAMPLE_RATE_CONVERTER_IN_DRIVER
		ds_now = __downsample_to_user(
#else
		ds_now = __i2s_copy_to_user(
#endif
					ads,
					buf, size,
					sg_virt(&sgl[i]), sgl[i].length,
					&bc_now);

		if (!ds_now && !sg_is_last(sgl + i)) {

			BUG_ON(bc_now);
			BUG_ON(take_two);
			take_two = true;

			/* The assumption is that this sgl entry is at the end
			 * of the fifo, and there isn't enough space till the
			 * end of the fifo for at least one target sample to be
			 * generated.  When this happens, we copy enough bytes
			 * from the next sgl entry to the end of the buffer of
			 * the current one, knowing that the copied bytes will
			 * cause the fifo to wrap around.  We adjust the next
			 * entry, and continue with the loop.
			 */

			BUG_ON(sg_virt(&sgl[i]) + sgl[i].length !=
				ads->in.buffer + kfifo_size(&ads->in.fifo));

			if (sgl[i + 1].length < PCM_IN_BUFFER_PADDING) {
				pr_debug("%s: not enough data till end of fifo\n",
						__func__);
				return 0;
			}

			memcpy(sg_virt(&sgl[i]) + sgl[i].length,
				sg_virt(&sgl[i + 1]),
				PCM_IN_BUFFER_PADDING);
			sgl[i].length += PCM_IN_BUFFER_PADDING;

			sg_set_buf(&sgl[i + 1],
				sg_virt(&sgl[i + 1]) + PCM_IN_BUFFER_PADDING,
				sgl[i + 1].length - PCM_IN_BUFFER_PADDING);

			goto again;
		}

		bytes_ds += ds_now;
		buf += ds_now;
		BUG_ON(ds_now > size);
		size -= ds_now;
		bytes_consumed_from_fifo += bc_now;
		pr_debug("%s: downsampled (%d req, %d actual)" \
			" -> total ds %d (size %d)\n", __func__,
			sgl[i].length, bytes_consumed_from_fifo,
			bytes_ds, size);
		if (sg_is_last(sgl + i))
			break;
	}

	kfifo_dma_out_finish(&ads->in.fifo, bytes_consumed_from_fifo);

	return bytes_ds;
}

static ssize_t tegra_audio_read(struct file *file, char __user *buf,
			size_t size, loff_t *off)
{
	ssize_t rc, total = 0;
	ssize_t nr;

	struct audio_driver_state *ads = ads_from_misc_in(file);

	mutex_lock(&ads->in.lock);

	if (!IS_ALIGNED(size, 4)) {
		pr_err("%s: user size request %d not aligned to 4\n",
			__func__, size);
		rc = -EINVAL;
		goto done_err;
	}

	pr_debug("%s:%d: read %d bytes, %d available\n", __func__,
			smp_processor_id(),
			size, kfifo_len(&ads->in.fifo));

again:
	/* If we want recording to stop immediately after it gets cancelled,
	 * then we do not want to wait for the fifo to get drained.
	 */
	if (ads->in.stop /* && kfifo_is_empty(&ads->in.fifo) */) {
		pr_debug("%s: recording has been cancelled (%d/%d bytes)\n",
				__func__, total, size);
		goto done_ok;
	}

	rc = start_recording_if_necessary(&ads->in);
	if (rc < 0 && rc != -EALREADY) {
		pr_err("%s: could not start recording\n", __func__);
		goto done_err;
	}

	ads->in.active = true;

	nr = 0;
	do {
		nr = downsample_to_user(ads, buf + total, size - total);
		if (nr < 0) {
			rc = nr;
			goto done_err;
		}
		total += nr;
	} while (nr);

	pr_debug("%s: copied %d bytes to user, total %d/%d\n",
			__func__, nr, total, size);

	if (total < size) {
		mutex_unlock(&ads->in.lock);
		pr_debug("%s: sleep (user %d total %d nr %d)\n", __func__,
				size, total, nr);
		rc = wait_for_completion_interruptible(
				&ads->in.fifo_completion);
		pr_debug("%s: awake\n", __func__);
		mutex_lock(&ads->in.lock);
		if (rc == -ERESTARTSYS) {
			pr_warn("%s: interrupted\n", __func__);
			goto done_err;
		}
		goto again;
	}

	pr_debug("%s: done reading %d bytes, %d available\n", __func__,
			total, kfifo_avail(&ads->in.fifo));

done_ok:
	rc = total;
	*off += total;

done_err:
	mutex_unlock(&ads->in.lock);
	return rc;
}

static int tegra_audio_out_open(struct inode *inode, struct file *file)
{
	struct audio_driver_state *ads = ads_from_misc_out(file);

	pr_debug("%s\n", __func__);

	mutex_lock(&ads->out.lock);
	if (!ads->out.opened++) {
		pr_debug("%s: resetting fifo and error count\n", __func__);
		ads->out.stop = false;
		memset(&ads->out.errors, 0, sizeof(ads->out.errors));
		kfifo_reset(&ads->out.fifo);
	}
	mutex_unlock(&ads->out.lock);

	return 0;
}

static int tegra_audio_out_release(struct inode *inode, struct file *file)
{
	struct audio_driver_state *ads = ads_from_misc_out(file);

	pr_debug("%s\n", __func__);

	mutex_lock(&ads->out.lock);
	if (ads->out.opened)
		ads->out.opened--;
	if (!ads->out.opened) {
		stop_playback_if_necessary(&ads->out);
		if (kfifo_len(&ads->out.fifo))
			pr_err("%s: output fifo is not empty (%d bytes left)\n",
				__func__, kfifo_len(&ads->out.fifo));
		allow_suspend(&ads->out);
	}
	mutex_unlock(&ads->out.lock);

	return 0;
}

static int tegra_audio_in_open(struct inode *inode, struct file *file)
{
	struct audio_driver_state *ads = ads_from_misc_in(file);

	pr_debug("%s\n", __func__);

	mutex_lock(&ads->in.lock);
	if (!ads->in.opened++) {
		pr_debug("%s: resetting fifo\n", __func__);
		/* By default, do not start recording when someone reads from
		 * input device.
		 */
		ads->in.stop = false;
		memset(&ads->in.errors, 0, sizeof(ads->in.errors));
		kfifo_reset(&ads->in.fifo);
	}
	mutex_unlock(&ads->in.lock);

	pr_debug("%s: done\n", __func__);
	return 0;
}

static int tegra_audio_in_release(struct inode *inode, struct file *file)
{
	struct audio_driver_state *ads = ads_from_misc_in(file);

	pr_debug("%s\n", __func__);

	mutex_lock(&ads->in.lock);
	if (ads->in.opened)
		ads->in.opened--;

	if (!ads->in.opened) {
		if (ads->in.active)
			request_stop_nosync(&ads->in);
		if (kfifo_len(&ads->in.fifo))
			pr_err("%s: input fifo is not empty (%d bytes left)\n",
				__func__, kfifo_len(&ads->in.fifo));
		allow_suspend(&ads->in);
	}

	mutex_unlock(&ads->in.lock);
	pr_debug("%s: done\n", __func__);
	return 0;
}

static const struct file_operations tegra_audio_out_fops = {
	.owner = THIS_MODULE,
	.open = tegra_audio_out_open,
	.release = tegra_audio_out_release,
	.write = tegra_audio_write,
};

static const struct file_operations tegra_audio_in_fops = {
	.owner = THIS_MODULE,
	.open = tegra_audio_in_open,
	.read = tegra_audio_read,
	.release = tegra_audio_in_release,
};

static int tegra_audio_ctl_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int tegra_audio_ctl_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tegra_audio_out_ctl_fops = {
	.owner = THIS_MODULE,
	.open = tegra_audio_ctl_open,
	.release = tegra_audio_ctl_release,
	.unlocked_ioctl = tegra_audio_out_ioctl,
};

static const struct file_operations tegra_audio_in_ctl_fops = {
	.owner = THIS_MODULE,
	.open = tegra_audio_ctl_open,
	.release = tegra_audio_ctl_release,
	.unlocked_ioctl = tegra_audio_in_ioctl,
};

static const struct file_operations tegra_audio_ctl_fops = {
	.owner = THIS_MODULE,
	.open = tegra_audio_ctl_open,
	.release = tegra_audio_ctl_release,
	.unlocked_ioctl = tegra_audio_ioctl,
};

static int init_stream_buffer(struct audio_stream *s,
				struct tegra_audio_buf_config *cfg,
				unsigned padding)
{
	pr_debug("%s (size %d threshold %d chunk %d)\n", __func__,
		cfg->size, cfg->threshold, cfg->chunk);

	if (cfg->chunk < PCM_DMA_CHUNK_MIN_SIZE_ORDER) {
		pr_err("%s: chunk %d too small (%d min)\n", __func__,
			cfg->chunk, PCM_DMA_CHUNK_MIN_SIZE_ORDER);
		return -EINVAL;
	}

	if (cfg->chunk > cfg->size) {
		pr_err("%s: chunk %d > size %d\n", __func__,
				cfg->chunk, cfg->size);
		return -EINVAL;
	}

	if (cfg->threshold > cfg->size) {
		pr_err("%s: threshold %d > size %d\n", __func__,
				cfg->threshold, cfg->size);
		return -EINVAL;
	}

	if ((1 << cfg->size) < padding) {
		pr_err("%s: size %d < buffer padding %d (bytes)\n", __func__,
			cfg->size, padding);
		return -EINVAL;
	}

	if (cfg->size > PCM_BUFFER_MAX_SIZE_ORDER) {
		pr_err("%s: size %d exceeds max %d\n", __func__,
			cfg->size, PCM_BUFFER_MAX_SIZE_ORDER);
		return -EINVAL;
	}

	if (!s->buffer) {
		pr_debug("%s: allocating buffer (size %d, padding %d)\n",
				__func__, 1 << cfg->size, padding);
		s->buffer = kmalloc((1 << cfg->size) + padding,
				GFP_KERNEL | GFP_DMA);
	}
	if (!s->buffer) {
		pr_err("%s: could not allocate output buffer\n", __func__);
		return -ENOMEM;
	}

	kfifo_init(&s->fifo, s->buffer, 1 << cfg->size);
	sg_init_table(&s->sg, 1);
	return 0;
}


static int setup_misc_device(struct miscdevice *misc,
			const struct file_operations  *fops,
			const char *fmt, ...)
{
	int rc = 0;
	va_list args;
	const int sz = 64;

	va_start(args, fmt);

	memset(misc, 0, sizeof(*misc));
	misc->minor = MISC_DYNAMIC_MINOR;
	misc->name  = kmalloc(sz, GFP_KERNEL);
	if (!misc->name) {
		rc = -ENOMEM;
		goto done;
	}

	vsnprintf((char *)misc->name, sz, fmt, args);
	misc->fops = fops;
	if (misc_register(misc)) {
		pr_err("%s: could not register %s\n", __func__, misc->name);
		kfree(misc->name);
		rc = -EIO;
		goto done;
	}

done:
	va_end(args);
	return rc;
}

static ssize_t dma_toggle_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct tegra_audio_platform_data *pdata = dev->platform_data;
	struct audio_driver_state *ads = pdata->driver_data;
	return sprintf(buf, "%s\n", ads->using_dma ? "dma" : "pio");
}

static ssize_t dma_toggle_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int use_dma;
	struct tegra_audio_platform_data *pdata = dev->platform_data;
	struct audio_driver_state *ads = pdata->driver_data;

	if (count < 4)
		return -EINVAL;

	use_dma = 0;
	if (!strncmp(buf, "dma", 3))
		use_dma = 1;
	else if (strncmp(buf, "pio", 3)) {
		dev_err(dev, "%s: invalid string [%s]\n", __func__, buf);
		return -EINVAL;
	}

	mutex_lock(&ads->out.lock);
	mutex_lock(&ads->in.lock);
	if (kfifo_len(&ads->out.fifo) || ads->in.active) {
		dev_err(dev, "%s: playback or recording in progress.\n",
			__func__);
		mutex_unlock(&ads->in.lock);
		mutex_unlock(&ads->out.lock);
		return -EBUSY;
	}
	if (!!use_dma ^ !!ads->using_dma)
		toggle_dma(ads);
	else
		dev_info(dev, "%s: no change\n", __func__);
	mutex_unlock(&ads->in.lock);
	mutex_unlock(&ads->out.lock);

	return count;
}

static DEVICE_ATTR(dma_toggle, 0644, dma_toggle_show, dma_toggle_store);

static ssize_t __attr_fifo_atn_read(char *buf, int atn_lvl)
{
	switch (atn_lvl) {
	case I2S_FIFO_ATN_LVL_ONE_SLOT:
		strncpy(buf, "1\n", 2);
		return 2;
	case I2S_FIFO_ATN_LVL_FOUR_SLOTS:
		strncpy(buf, "4\n", 2);
		return 2;
	case I2S_FIFO_ATN_LVL_EIGHT_SLOTS:
		strncpy(buf, "8\n", 2);
		return 2;
	case I2S_FIFO_ATN_LVL_TWELVE_SLOTS:
		strncpy(buf, "12\n", 3);
		return 3;
	default:
		BUG_ON(1);
		return -EIO;
	}
}

static ssize_t __attr_fifo_atn_write(struct audio_driver_state *ads,
		struct audio_stream *as,
		int *fifo_lvl,
		const char *buf, size_t size)
{
	int lvl;

	if (size > 3) {
		pr_err("%s: buffer size %d too big\n", __func__, size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &lvl) != 1) {
		pr_err("%s: invalid input string [%s]\n", __func__, buf);
		return -EINVAL;
	}

	switch (lvl) {
	case 1:
		lvl = I2S_FIFO_ATN_LVL_ONE_SLOT;
		break;
	case 4:
		lvl = I2S_FIFO_ATN_LVL_FOUR_SLOTS;
		break;
	case 8:
		lvl = I2S_FIFO_ATN_LVL_EIGHT_SLOTS;
		break;
	case 12:
		lvl = I2S_FIFO_ATN_LVL_TWELVE_SLOTS;
		break;
	default:
		pr_err("%s: invalid attention level %d\n", __func__, lvl);
		return -EINVAL;
	}

	*fifo_lvl = lvl;
	pr_info("%s: fifo level %d\n", __func__, *fifo_lvl);

	return size;
}

static ssize_t tx_fifo_atn_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct tegra_audio_platform_data *pdata = dev->platform_data;
	struct audio_driver_state *ads = pdata->driver_data;
	return __attr_fifo_atn_read(buf, ads->out.i2s_fifo_atn_level);
}

static ssize_t tx_fifo_atn_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	ssize_t rc;
	struct tegra_audio_platform_data *pdata = dev->platform_data;
	struct audio_driver_state *ads = pdata->driver_data;
	mutex_lock(&ads->out.lock);
	if (kfifo_len(&ads->out.fifo)) {
		pr_err("%s: playback in progress.\n", __func__);
		rc = -EBUSY;
		goto done;
	}
	rc = __attr_fifo_atn_write(ads, &ads->out,
			&ads->out.i2s_fifo_atn_level,
			buf, count);
done:
	mutex_unlock(&ads->out.lock);
	return rc;
}

static DEVICE_ATTR(tx_fifo_atn, 0644, tx_fifo_atn_show, tx_fifo_atn_store);

static ssize_t rx_fifo_atn_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct tegra_audio_platform_data *pdata = dev->platform_data;
	struct audio_driver_state *ads = pdata->driver_data;
	return __attr_fifo_atn_read(buf, ads->in.i2s_fifo_atn_level);
}

static ssize_t rx_fifo_atn_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	ssize_t rc;
	struct tegra_audio_platform_data *pdata = dev->platform_data;
	struct audio_driver_state *ads = pdata->driver_data;
	mutex_lock(&ads->in.lock);
	if (ads->in.active) {
		pr_err("%s: recording in progress.\n", __func__);
		rc = -EBUSY;
		goto done;
	}
	rc = __attr_fifo_atn_write(ads, &ads->in,
			&ads->in.i2s_fifo_atn_level,
			buf, count);
done:
	mutex_unlock(&ads->in.lock);
	return rc;
}

static DEVICE_ATTR(rx_fifo_atn, 0644, rx_fifo_atn_show, rx_fifo_atn_store);

static int tegra_audio_probe(struct platform_device *pdev)
{
	int rc;
	struct resource *res;
	struct clk *i2s_clk, *audio_sync_clk, *dap_mclk;
	struct audio_driver_state *state;

	pr_info("%s\n", __func__);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->pdev = pdev;
	state->pdata = pdev->dev.platform_data;
	state->pdata->driver_data = state;
	BUG_ON(!state->pdata);

	if (!(state->pdata->mask &
			(TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX))) {
		dev_err(&pdev->dev, "neither tx nor rx is enabled!\n");
		return -EIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource!\n");
		return -ENODEV;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "memory region already claimed!\n");
		return -ENOMEM;
	}

	state->i2s_phys = res->start;
	state->i2s_base = (unsigned long)ioremap(res->start,
			res->end - res->start + 1);
	if (!state->i2s_base) {
		dev_err(&pdev->dev, "cannot remap iomem!\n");
		return -EIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(&pdev->dev, "no dma resource!\n");
		return -ENODEV;
	}
	state->dma_req_sel = res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "no irq resource!\n");
		return -ENODEV;
	}
	state->irq = res->start;

	memset(&state->pio_stats, 0, sizeof(state->pio_stats));

	i2s_clk = clk_get(&pdev->dev, NULL);
	if (!i2s_clk) {
		dev_err(&pdev->dev, "%s: could not get i2s clock\n",
			__func__);
		return -EIO;
	}

	clk_set_rate(i2s_clk, state->pdata->i2s_clk_rate);
	if (clk_enable(i2s_clk)) {
		dev_err(&pdev->dev, "%s: failed to enable i2s clock\n",
			__func__);
		return -EIO;
	}
	pr_info("%s: i2s_clk rate %ld\n", __func__, clk_get_rate(i2s_clk));

	dap_mclk = tegra_get_clock_by_name(state->pdata->dap_clk);
	if (!dap_mclk) {
		dev_err(&pdev->dev, "%s: could not get DAP clock\n",
			__func__);
		return -EIO;
	}
	clk_enable(dap_mclk);

	audio_sync_clk = tegra_get_clock_by_name(state->pdata->audio_sync_clk);
	if (!audio_sync_clk) {
		dev_err(&pdev->dev, "%s: could not get audio_2x clock\n",
			__func__);
		return -EIO;
	}
	clk_enable(audio_sync_clk);

	rc = i2s_configure(pdev);
	if (rc < 0)
		return rc;

	if ((state->pdata->mask & TEGRA_AUDIO_ENABLE_TX)) {
		state->out.opened = 0;
		state->out.active = false;
		mutex_init(&state->out.lock);
		init_completion(&state->out.fifo_completion);
		init_completion(&state->out.stop_completion);
		spin_lock_init(&state->out.dma_req_lock);
		state->out.buf_phys = 0;
		state->out.dma_chan = NULL;
		state->out.dma_has_it = false;

		state->out.i2s_fifo_atn_level = I2S_FIFO_ATN_LVL_FOUR_SLOTS;
		state->out.buffer = 0;
		state->out.buf_config.size = PCM_BUFFER_MAX_SIZE_ORDER;
		state->out.buf_config.threshold = PCM_BUFFER_THRESHOLD_ORDER;
		state->out.buf_config.chunk = PCM_BUFFER_DMA_CHUNK_SIZE_ORDER;
		rc = init_stream_buffer(&state->out, &state->out.buf_config, 0);
		if (rc < 0)
			return rc;

		INIT_WORK(&state->out.allow_suspend_work, allow_suspend_worker);
		pm_qos_add_request(&state->out.pm_qos, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

		rc = setup_misc_device(&state->misc_out,
			&tegra_audio_out_fops,
			"audio%d_out", state->pdev->id);
		if (rc < 0)
			return rc;

		rc = setup_misc_device(&state->misc_out_ctl,
				&tegra_audio_out_ctl_fops,
				"audio%d_out_ctl", state->pdev->id);
		if (rc < 0)
			return rc;
	}

	if ((state->pdata->mask & TEGRA_AUDIO_ENABLE_RX)) {
		state->in.opened = 0;
		state->in.active = false;
		mutex_init(&state->in.lock);
		init_completion(&state->in.fifo_completion);
		init_completion(&state->in.stop_completion);
		spin_lock_init(&state->in.dma_req_lock);
		state->in.buf_phys = 0;
		state->in.dma_chan = NULL;
		state->in.dma_has_it = false;

		state->in.i2s_fifo_atn_level = I2S_FIFO_ATN_LVL_FOUR_SLOTS;
		state->in.buffer = 0;
		state->in.buf_config.size = PCM_BUFFER_MAX_SIZE_ORDER;
		state->in.buf_config.threshold = PCM_BUFFER_THRESHOLD_ORDER;
		state->in.buf_config.chunk = PCM_BUFFER_DMA_CHUNK_SIZE_ORDER;
		rc = init_stream_buffer(&state->in, &state->in.buf_config,
				PCM_IN_BUFFER_PADDING);
		if (rc < 0)
			return rc;

		INIT_WORK(&state->in.allow_suspend_work, allow_suspend_worker);
		pm_qos_add_request(&state->in.pm_qos, PM_QOS_CPU_DMA_LATENCY,
					PM_QOS_DEFAULT_VALUE);

		rc = setup_misc_device(&state->misc_in,
			&tegra_audio_in_fops,
			"audio%d_in", state->pdev->id);
		if (rc < 0)
			return rc;

		rc = setup_misc_device(&state->misc_in_ctl,
			&tegra_audio_in_ctl_fops,
			"audio%d_in_ctl", state->pdev->id);
		if (rc < 0)
			return rc;
	}

	if (request_irq(state->irq, i2s_interrupt,
			IRQF_DISABLED, state->pdev->name, state) < 0) {
		dev_err(&pdev->dev,
			"%s: could not register handler for irq %d\n",
			__func__, state->irq);
		return -EIO;
	}

	rc = setup_misc_device(&state->misc_ctl,
			&tegra_audio_ctl_fops,
			"audio%d_ctl", state->pdev->id);
	if (rc < 0)
		return rc;

	state->using_dma = state->pdata->dma_on;
	if (!state->using_dma)
		sound_ops = &pio_sound_ops;
	sound_ops->setup(state);

	rc = device_create_file(&pdev->dev, &dev_attr_dma_toggle);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s: could not create sysfs entry %s: %d\n",
			__func__, dev_attr_dma_toggle.attr.name, rc);
		return rc;
	}

	rc = device_create_file(&pdev->dev, &dev_attr_tx_fifo_atn);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s: could not create sysfs entry %s: %d\n",
			__func__, dev_attr_tx_fifo_atn.attr.name, rc);
		return rc;
	}

	rc = device_create_file(&pdev->dev, &dev_attr_rx_fifo_atn);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s: could not create sysfs entry %s: %d\n",
			__func__, dev_attr_rx_fifo_atn.attr.name, rc);
		return rc;
	}

	state->in_config.rate = 11025;
	state->in_config.stereo = false;
	state->in_divs = divs_11025;
	state->in_divs_len = ARRAY_SIZE(divs_11025);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_audio_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	/* dev_info(&pdev->dev, "%s\n", __func__); */
	return 0;
}

static int tegra_audio_resume(struct platform_device *pdev)
{
	return i2s_configure(pdev);
}
#endif /* CONFIG_PM */

static struct platform_driver tegra_audio_driver = {
	.driver = {
		.name = "i2s",
		.owner = THIS_MODULE,
	},
	.probe = tegra_audio_probe,
#ifdef CONFIG_PM
	.suspend = tegra_audio_suspend,
	.resume = tegra_audio_resume,
#endif
};

static int __init tegra_audio_init(void)
{
	return platform_driver_register(&tegra_audio_driver);
}

module_init(tegra_audio_init);
MODULE_LICENSE("GPL");
