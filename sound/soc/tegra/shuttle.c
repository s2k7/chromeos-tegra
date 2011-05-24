/*
 * shuttle.c - Shuttle machine ASoC driver
 *
 * Written by Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * Based on code copyright/by:
 *
 * (c) 2009, 2010 Nvidia Graphics Pvt. Ltd.
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <asm/mach-types.h>

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <mach/shuttle_audio.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_das.h"
#include "tegra_i2s.h"
#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

#define DRV_NAME "tegra-snd-shuttle"

struct tegra_shuttle {
	struct tegra_asoc_utils_data util_data;
	struct shuttle_audio_platform_data* pdata;
};

static int shuttle_asoc_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_shuttle *shuttle = snd_soc_card_get_drvdata(card);
	int srate, mclk, mclk_change;
	int err;

	srate = params_rate(params);
	switch (srate) {
	case 64000:
	case 88200:
	case 96000:
		mclk = 128 * srate;
		break;
	default:
		mclk = 256 * srate;
		break;
	}
	/* FIXME: Codec only requires >= 3MHz if OSR==0 */
	while (mclk < 6000000)
		mclk *= 2;

	err = tegra_asoc_utils_set_rate(&shuttle->util_data, srate, mclk,
					&mclk_change);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	if (mclk_change) {
		err = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
					     SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "codec_dai clock not set\n");
			return err;
		}
	}

	return 0;
}

static struct snd_soc_ops shuttle_asoc_ops = {
	.hw_params = shuttle_asoc_hw_params,
};

static int shuttle_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_shuttle *shuttle = snd_soc_card_get_drvdata(card);
	int srate, mclk, mclk_change;
	int err;

	/*
	 * FIXME: Refactor mclk into PCM-specific function; SPDIF doesn't
	 * need it
	 */
	srate = params_rate(params);
	switch (srate) {
	case 64000:
	case 88200:
	case 96000:
		mclk = 128 * srate;
		break;
	default:
		mclk = 256 * srate;
		break;
	}
	/* FIXME: Codec only requires >= 3MHz if OSR==0 */
	while (mclk < 6000000)
		mclk *= 2;

	err = tegra_asoc_utils_set_rate(&shuttle->util_data, srate, mclk,
					&mclk_change);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	return 0;
}

static struct snd_soc_ops shuttle_spdif_ops = {
	.hw_params = shuttle_spdif_hw_params,
};

static struct snd_soc_jack shuttle_jack;
static struct snd_soc_jack_pin shuttle_jack_pins[] = {
	/* Disable speaker when headphone is plugged in */
	{
		.pin = "Internal Speaker",
		.mask = SND_JACK_HEADPHONE,
	},
};

static struct snd_soc_jack_gpio shuttle_jack_gpios[] = {
	{
		.name = "headphone detect",
		.report = SND_JACK_HEADPHONE,
		.debounce_time = 150,
	}
};

static const struct snd_soc_dapm_widget shuttle_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Internal Speaker", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
};

static const struct snd_soc_dapm_route shuttle_audio_map[] = {
	{"Headphone Jack", NULL, "HPR"},
	{"Headphone Jack", NULL, "HPL"},
	{"Internal Speaker", NULL, "SPKL"},
	{"Internal Speaker", NULL, "SPKLN"},
	{"Internal Speaker", NULL, "SPKR"},
	{"Internal Speaker", NULL, "SPKRN"},
	{"Mic Bias1", NULL, "Internal Mic"},
	{"MIC1", NULL, "Mic Bias1"},
};

static const struct snd_kcontrol_new shuttle_controls[] = {
	SOC_DAPM_PIN_SWITCH("Internal Speaker"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
};

static int shuttle_asoc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct tegra_shuttle *shuttle = snd_soc_card_get_drvdata(card);
	struct shuttle_audio_platform_data* pdata = shuttle->pdata;
	int ret;

	/* Store the GPIO used to detect headphone */
	shuttle_jack_gpios[0].gpio = pdata->gpio_hp_det;
	
	/* Add Shuttle specific widgets */
	snd_soc_dapm_new_controls(dapm, shuttle_dapm_widgets,
					ARRAY_SIZE(shuttle_dapm_widgets));
	
	/* Add Shuttle specific controls */
	ret = snd_soc_add_controls(codec, shuttle_controls,
				   ARRAY_SIZE(shuttle_controls));
	if (ret < 0)
		return ret;

	/* Setup Shuttle specific audio paths */
	snd_soc_dapm_add_routes(dapm, shuttle_audio_map,
				ARRAY_SIZE(shuttle_audio_map));

	/* Set endpoints to not connected */
	snd_soc_dapm_nc_pin(dapm, "LINEL");
	snd_soc_dapm_nc_pin(dapm, "LINEH");
	snd_soc_dapm_nc_pin(dapm, "PHONEIN");
	snd_soc_dapm_nc_pin(dapm, "MIC2");
	snd_soc_dapm_nc_pin(dapm, "MONO");

	/* Set endpoints to default off mode */
	snd_soc_dapm_enable_pin(dapm, "Internal Speaker");
	snd_soc_dapm_enable_pin(dapm, "Internal Mic");
	snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
	
	ret = snd_soc_dapm_sync(dapm);
	if (ret)
		return ret;
	
	/* Headphone jack detection */		
	ret = snd_soc_jack_new(codec, "Headphone Jack", SND_JACK_HEADPHONE,
			 &shuttle_jack);
	if (ret)
		return ret;
			 
	ret = snd_soc_jack_add_pins(&shuttle_jack,
			      ARRAY_SIZE(shuttle_jack_pins),
			      shuttle_jack_pins);
	if (ret)
		return ret;
				  
	return snd_soc_jack_add_gpios(&shuttle_jack,
			       ARRAY_SIZE(shuttle_jack_gpios),
			       shuttle_jack_gpios);
}

static struct snd_soc_dai_link shuttle_links[] = {
	{
		.name = "ALC5624",
		.stream_name = "ALC5624 PCM",
		.codec_name = "alc5624.0-0018",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra-i2s.0",
		.codec_dai_name = "alc5624-hifi",
		.init = shuttle_asoc_init,
		.ops = &shuttle_asoc_ops,
	},
	{
		.name = "SPDIF",
		.stream_name = "spdif",
		.codec_name = "spdif-dit",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra-spdif",
		.codec_dai_name = "dit-hifi",
		.ops = &shuttle_spdif_ops,
	},
};

static struct snd_soc_card snd_soc_shuttle = {
	.name = "tegra-shuttle",
	.dai_link = shuttle_links,
	.num_links = ARRAY_SIZE(shuttle_links),
};

static __devinit int tegra_snd_shuttle_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_shuttle;
	struct tegra_shuttle *shuttle;
	struct shuttle_audio_platform_data* pdata;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data supplied\n");
		return -EINVAL;
	}
	
	shuttle = kzalloc(sizeof(struct tegra_shuttle), GFP_KERNEL);
	if (!shuttle) {
		dev_err(&pdev->dev, "Can't allocate tegra_shuttle\n");
		return -ENOMEM;
	}
	
	shuttle->pdata = pdata;

	ret = tegra_asoc_utils_init(&shuttle->util_data, &pdev->dev);
	if (ret)
		goto err_free_shuttle;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, shuttle);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_clear_drvdata;
	}

	return 0;

err_clear_drvdata:
	snd_soc_card_set_drvdata(card, NULL);
	platform_set_drvdata(pdev, NULL);
	card->dev = NULL;
	tegra_asoc_utils_fini(&shuttle->util_data);
err_free_shuttle:
	kfree(shuttle);
	return ret;
}

static int __devexit tegra_snd_shuttle_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_shuttle *shuttle = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	snd_soc_card_set_drvdata(card, NULL);
	platform_set_drvdata(pdev, NULL);
	card->dev = NULL;

	tegra_asoc_utils_fini(&shuttle->util_data);
	kfree(shuttle);

	return 0;
}

static struct platform_driver tegra_snd_shuttle_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_snd_shuttle_probe,
	.remove = __devexit_p(tegra_snd_shuttle_remove),
};

static int __init snd_tegra_shuttle_init(void)
{
	return platform_driver_register(&tegra_snd_shuttle_driver);
}
module_init(snd_tegra_shuttle_init);

static void __exit snd_tegra_shuttle_exit(void)
{
	platform_driver_unregister(&tegra_snd_shuttle_driver);
}
module_exit(snd_tegra_shuttle_exit);

MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_DESCRIPTION("Shuttle machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
