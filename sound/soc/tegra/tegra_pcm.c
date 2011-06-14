/*
 * tegra_pcm.c  --  ALSA Soc Audio Layer
 *
 * (c) 2009 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * (c) 2004-2005 Simtec Electronics
 *    http://armlinux.simtec.co.uk/
 *    Ben Dooks <ben@simtec.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include "tegra_soc.h"

#define PLAYBACK_STARTED true
#define PLAYBACK_STOPPED false

static void start_i2s_playback(void)
{
	i2s_fifo_set_attention_level(I2S_IFC, I2S_FIFO_TX,
					I2S_FIFO_ATN_LVL_FOUR_SLOTS);
	i2s_fifo_enable(I2S_IFC, I2S_FIFO_TX, 1);
}

static void stop_i2s_playback(void)
{
	int spin = 0;
	i2s_set_fifo_irq_on_err(I2S_IFC, I2S_FIFO_TX, 0);
	i2s_set_fifo_irq_on_qe(I2S_IFC, I2S_FIFO_TX, 0);
	i2s_fifo_enable(I2S_IFC, I2S_FIFO_TX, 0);
	while ((i2s_get_status(I2S_IFC) & I2S_I2S_FIFO_TX_BUSY) &&
		spin < 100)
		if (spin++ > 50)
			pr_info("%s: spin %d\n", __func__, spin);
	if (spin == 100)
		pr_warn("%s: spinny\n", __func__);
}

static void tegra_pcm_play(struct tegra_runtime_data *prtd)
{
	static int reqid;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	if (runtime->dma_addr) {
		prtd->size = frames_to_bytes(runtime, runtime->period_size);
		if (reqid == 0) {
			prtd->dma_req1.source_addr = buf->addr +
				+ frames_to_bytes(runtime, prtd->dma_pos);
			prtd->dma_req1.size = prtd->size;
			tegra_dma_enqueue_req(prtd->dma_chan, &prtd->dma_req1);
			reqid = 1;
		} else {
			prtd->dma_req2.source_addr = buf->addr +
				+ frames_to_bytes(runtime, prtd->dma_pos);
			prtd->dma_req2.size = prtd->size;
			tegra_dma_enqueue_req(prtd->dma_chan, &prtd->dma_req2);
			reqid = 0;
		}
	}

	prtd->dma_pos += runtime->period_size;
	if (prtd->dma_pos >= runtime->buffer_size)
		prtd->dma_pos = 0;

}

static void dma_tx_complete_callback(struct tegra_dma_req *req)
{
	struct tegra_runtime_data *prtd = (struct tegra_runtime_data *)req->dev;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (++prtd->period_index >= runtime->periods)
		prtd->period_index = 0;

	snd_pcm_period_elapsed(substream);
	tegra_pcm_play(prtd);
}

static void setup_dma_tx_request(struct tegra_dma_req *req)
{
	memset(req, 0, sizeof(*req));
	req->complete = dma_tx_complete_callback;
	req->to_memory = false;
	req->dest_addr = i2s_get_fifo_phy_base(I2S_IFC, I2S_FIFO_TX);
	req->dest_wrap = 4;
	req->source_bus_width = 32;
	req->source_wrap = 0;
	req->dest_bus_width = 32;
	req->req_sel = I2S_IFC ? 1 : 2; /* 1 = I2S2, 2 = I2S1 */
}

/* recording */
static void start_i2s_capture(void)
{
	i2s_fifo_set_attention_level(I2S_IFC, I2S_FIFO_RX,
					I2S_FIFO_ATN_LVL_FOUR_SLOTS);
	i2s_fifo_enable(I2S_IFC, I2S_FIFO_RX, 1);
}

static void stop_i2s_capture(void)
{
	int spin = 0;
	i2s_set_fifo_irq_on_err(I2S_IFC, I2S_FIFO_RX, 0);
	i2s_set_fifo_irq_on_qe(I2S_IFC, I2S_FIFO_RX, 0);
	i2s_fifo_enable(I2S_IFC, I2S_FIFO_RX, 0);
	while ((i2s_get_status(I2S_IFC) & I2S_I2S_FIFO_RX_BUSY) &&
		spin < 100)
		if (spin++ > 50)
			pr_info("%s: spin %d\n", __func__, spin);
	if (spin == 100)
		pr_warn("%s: spinny\n", __func__);
}

static void tegra_pcm_capture(struct tegra_runtime_data *prtd)
{
	static int reqid;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	if (runtime->dma_addr) {
		prtd->size = frames_to_bytes(runtime, runtime->period_size);
		if (reqid == 0) {
			prtd->dma_req1.dest_addr = buf->addr +
				+ frames_to_bytes(runtime, prtd->dma_pos);
			prtd->dma_req1.size = prtd->size;
			tegra_dma_enqueue_req(prtd->dma_chan, &prtd->dma_req1);
			reqid = 1;
		} else {
			prtd->dma_req2.dest_addr = buf->addr +
				+ frames_to_bytes(runtime, prtd->dma_pos);
			prtd->dma_req2.size = prtd->size;
			tegra_dma_enqueue_req(prtd->dma_chan, &prtd->dma_req2);
			reqid = 0;
		}
	}

	prtd->dma_pos += runtime->period_size;
	if (prtd->dma_pos >= runtime->buffer_size)
		prtd->dma_pos = 0;

}

static void dma_rx_complete_callback(struct tegra_dma_req *req)
{
	struct tegra_runtime_data *prtd = (struct tegra_runtime_data *)req->dev;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (++prtd->period_index >= runtime->periods)
		prtd->period_index = 0;

	snd_pcm_period_elapsed(substream);
	tegra_pcm_capture(prtd);
}

static void setup_dma_rx_request(struct tegra_dma_req *req)
{
	memset(req, 0, sizeof(*req));
	req->complete = dma_rx_complete_callback;
	req->to_memory = true;
	req->source_addr = i2s_get_fifo_phy_base(I2S_IFC, I2S_FIFO_RX);
	req->dest_wrap = 0;
	req->source_bus_width = 32;
	req->source_wrap = 4;
	req->dest_bus_width = 32;
	req->req_sel = I2S_IFC ? 1 : 2; /* 1 = I2S2, 2 = I2S1 */
}

static const struct snd_pcm_hardware tegra_pcm_hardware = {
	.info	= SNDRV_PCM_INFO_INTERLEAVED | \
			SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME | \
			SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID ,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min		= 2,
	.channels_max		= 2,
	.buffer_bytes_max	= (PAGE_SIZE * 8),
	.period_bytes_min	= 1024,
	.period_bytes_max	= (PAGE_SIZE),
	.periods_min		= 2,
	.periods_max		= 8,
	.fifo_size			= 4,
};

static int tegra_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int tegra_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int tegra_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int tegra_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct tegra_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			atomic_set(&prtd->state, STATE_INIT);
			tegra_pcm_play(prtd); /* dma enqueue req1 */
			tegra_pcm_play(prtd); /* dma enqueue req2 */
			start_i2s_playback();
		} else if (atomic_read(&prtd->state) != STATE_INIT) {
			/* start recording */
			atomic_set(&prtd->state, STATE_INIT);
			tegra_pcm_capture(prtd); /* dma enqueue req1 */
			tegra_pcm_capture(prtd); /* dma enqueue req2 */
			start_i2s_capture();
		}
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		atomic_set(&prtd->state, STATE_ABORT);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static snd_pcm_uframes_t tegra_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd = runtime->private_data;

	return prtd->period_index * runtime->period_size;
}

static int tegra_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd;
	int ret = 0;

	prtd = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	memset(prtd, 0, sizeof(*prtd));
	runtime->private_data = prtd;
	prtd->substream = substream;

	/* Enable the DAP outputs */
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2, TEGRA_TRI_NORMAL);

	/* Setup I2S clocks */
	prtd->i2s_clk = i2s_get_clock_by_name(I2S_NAME);
	if (!prtd->i2s_clk) {
		pr_err("%s: could not get i2s1 clock\n", __func__);
		return -EIO;
	}

	clk_set_rate(prtd->i2s_clk, I2S_CLK);
	if (clk_enable(prtd->i2s_clk)) {
		pr_err("%s: failed to enable i2s1 clock\n", __func__);
		return -EIO;
	}

	i2s_set_channel_bit_count(I2S_IFC, TEGRA_DEFAULT_SR,
					clk_get_rate(prtd->i2s_clk));

	prtd->dap_mclk = i2s_get_clock_by_name("clk_dev1");
	if (!prtd->dap_mclk) {
		pr_err("%s: could not get DAP clock\n", __func__);
		return -EIO;
	}
	clk_enable(prtd->dap_mclk);

	prtd->audio_sync_clk = i2s_get_clock_by_name("audio_2x");
	if (!prtd->audio_sync_clk) {
		pr_err("%s: could not get audio_2x clock\n", __func__);
		return -EIO;
	}
	clk_enable(prtd->audio_sync_clk);

	atomic_set(&prtd->state, STATE_INVALID);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		setup_dma_tx_request(&prtd->dma_req1);
		setup_dma_tx_request(&prtd->dma_req2);
	} else {
		setup_dma_rx_request(&prtd->dma_req1);
		setup_dma_rx_request(&prtd->dma_req2);
	}

	prtd->dma_req1.dev = prtd;
	prtd->dma_req2.dev = prtd;

	prtd->dma_chan = tegra_dma_allocate_channel(TEGRA_DMA_MODE_ONESHOT);
	if (IS_ERR(prtd->dma_chan)) {
		pr_err("%s: could not allocate DMA channel for I2S: %ld\n",
		       __func__, PTR_ERR(prtd->dma_chan));
		ret = PTR_ERR(prtd->dma_chan);
		goto fail;
	}

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, &tegra_pcm_hardware);

	goto end;

fail:
	atomic_set(&prtd->state, STATE_EXIT);

	if (prtd->dma_chan) {
		tegra_dma_flush(prtd->dma_chan);
		tegra_dma_free_channel(prtd->dma_chan);
	}
	/* Tristate the DAP pinmux */
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1, TEGRA_TRI_TRISTATE);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1, TEGRA_TRI_TRISTATE);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2, TEGRA_TRI_TRISTATE);

	kfree(prtd);

end:
	return ret;
}

static int tegra_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd = runtime->private_data;

	if (!prtd) {
		printk(KERN_ERR "tegra_pcm_close called with prtd == NULL\n");
		return 0;
	}

	atomic_set(&prtd->state, STATE_EXIT);

	if (prtd->dma_chan) {
		tegra_dma_dequeue_req(prtd->dma_chan, &prtd->dma_req1);
		tegra_dma_dequeue_req(prtd->dma_chan, &prtd->dma_req2);
		stop_i2s_playback();
		tegra_dma_flush(prtd->dma_chan);
		tegra_dma_free_channel(prtd->dma_chan);
		prtd->dma_chan = NULL;
	}

	kfree(prtd);

	return 0;
}

static int tegra_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops tegra_pcm_ops = {
	.open       = tegra_pcm_open,
	.close      = tegra_pcm_close,
	.ioctl      = snd_pcm_lib_ioctl,
	.hw_params  = tegra_pcm_hw_params,
	.hw_free    = tegra_pcm_hw_free,
	.prepare    = tegra_pcm_prepare,
	.trigger    = tegra_pcm_trigger,
	.pointer    = tegra_pcm_pointer,
	.mmap       = tegra_pcm_mmap,
};

static int tegra_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = tegra_pcm_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
						&buf->addr, GFP_KERNEL);
	buf->bytes = size;
	return 0;
}

static void tegra_pcm_deallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
					buf->area, buf->addr);
		buf->area = NULL;
	}
}

static void tegra_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		buf = &substream->dma_buffer;
		if (!buf) {
			printk(KERN_ERR "no buffer %d\n", stream);
			continue;
		}
		tegra_pcm_deallocate_dma_buffer(pcm, stream);
	}

}

static u64 tegra_dma_mask = DMA_BIT_MASK(32);

static int tegra_pcm_new(struct snd_card *card,
				struct snd_soc_dai *dai, struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &tegra_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (dai->playback.channels_min) {
		ret = tegra_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->capture.channels_min) {
		ret = tegra_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

struct snd_soc_platform tegra_soc_platform = {
	.name     = "tegra-audio",
	.pcm_ops  = &tegra_pcm_ops,
	.pcm_new  = tegra_pcm_new,
	.pcm_free = tegra_pcm_free_dma_buffers,
};
EXPORT_SYMBOL_GPL(tegra_soc_platform);

static int __init tegra_soc_platform_init(void)
{
	return snd_soc_register_platform(&tegra_soc_platform);
}
module_init(tegra_soc_platform_init);

static void __exit tegra_soc_platform_exit(void)
{
	snd_soc_unregister_platform(&tegra_soc_platform);
}
module_exit(tegra_soc_platform_exit);

MODULE_DESCRIPTION("Tegra PCM DMA module");
MODULE_LICENSE("GPL");
