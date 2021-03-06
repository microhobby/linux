/*
 * ASoC Driver for Allo DigiOne
 *
 * Author: Baswaraj <jaikumar@cemsolutions.net>
 *	   Copyright 2017
 * based on code by Daniel Matuschek <info@crazy-audio.com>
 * based on code by Florian Meier <florian.meier@koalo.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/gpio/consumer.h>

#include "../codecs/wm8804.h"

static short int auto_shutdown_output;
module_param(auto_shutdown_output, short,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(auto_shutdown_output, "Shutdown SP/DIF output if playback is stopped");

#define CLK_44EN_RATE 22579200UL
#define CLK_48EN_RATE 24576000UL

static struct gpio_desc *snd_allo_clk44gpio;
static struct gpio_desc *snd_allo_clk48gpio;

static int samplerate = 44100;

static uint32_t snd_allo_digione_enable_clock(int sample_rate)
{
	switch (sample_rate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		gpiod_set_value_cansleep(snd_allo_clk44gpio, 1);
		gpiod_set_value_cansleep(snd_allo_clk48gpio, 0);
		return CLK_44EN_RATE;
	default:
		gpiod_set_value_cansleep(snd_allo_clk48gpio, 1);
		gpiod_set_value_cansleep(snd_allo_clk44gpio, 0);
		return CLK_48EN_RATE;
	}
}


static int snd_allo_digione_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = rtd->codec_dai->component;

	/* enable TX output */
	snd_soc_component_update_bits(component, WM8804_PWRDN, 0x4, 0x0);

	return 0;
}

static int snd_allo_digione_startup(struct snd_pcm_substream *substream)
{
	/* turn on digital output */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = rtd->codec_dai->component;

	snd_soc_component_update_bits(component, WM8804_PWRDN, 0x3c, 0x00);
	return 0;
}

static void snd_allo_digione_shutdown(struct snd_pcm_substream *substream)
{
	/* turn off output */
	if (auto_shutdown_output) {
		/* turn off output */
		struct snd_soc_pcm_runtime *rtd = substream->private_data;
		struct snd_soc_component *component = rtd->codec_dai->component;

		snd_soc_component_update_bits(component, WM8804_PWRDN, 0x3c, 0x3c);
	}
}

static int snd_allo_digione_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_component *component = rtd->codec_dai->component;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	int sysclk = 27000000; /* This is fixed on this board */

	long mclk_freq = 0;
	int mclk_div = 1;
	int sampling_freq = 1;

	int ret;

	samplerate = params_rate(params);
	mclk_freq = samplerate * 256;
	mclk_div = WM8804_MCLKDIV_256FS;

	sysclk = snd_allo_digione_enable_clock(samplerate);

	switch (samplerate) {
	case 32000:
		sampling_freq = 0x03;
		break;
	case 44100:
		sampling_freq = 0x00;
		break;
	case 48000:
		sampling_freq = 0x02;
		break;
	case 88200:
		sampling_freq = 0x08;
		break;
	case 96000:
		sampling_freq = 0x0a;
		break;
	case 176400:
		sampling_freq = 0x0c;
		break;
	case 192000:
		sampling_freq = 0x0e;
		break;
	default:
		dev_err(rtd->card->dev,
		"Failed to set WM8804 SYSCLK, unsupported samplerate %d\n",
		samplerate);
	}

	snd_soc_dai_set_clkdiv(codec_dai, WM8804_MCLK_DIV, mclk_div);
	snd_soc_dai_set_pll(codec_dai, 0, 0, sysclk, mclk_freq);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8804_TX_CLKSRC_PLL,
					sysclk, SND_SOC_CLOCK_OUT);

	if (ret < 0) {
		dev_err(rtd->card->dev,
		"Failed to set WM8804 SYSCLK: %d\n", ret);
		return ret;
	}

	/* Enable TX output */
	snd_soc_component_update_bits(component, WM8804_PWRDN, 0x4, 0x0);

	/* Power on */
	snd_soc_component_update_bits(component, WM8804_PWRDN, 0x9, 0);

	/* set sampling frequency status bits */
	snd_soc_component_update_bits(component, WM8804_SPDTX4, 0x0f, sampling_freq);

	return snd_soc_dai_set_bclk_ratio(cpu_dai, 64);
}

/* machine stream operations */
static struct snd_soc_ops snd_allo_digione_ops = {
	.hw_params	= snd_allo_digione_hw_params,
	.startup	= snd_allo_digione_startup,
	.shutdown	= snd_allo_digione_shutdown,
};

static struct snd_soc_dai_link snd_allo_digione_dai[] = {
{
	.name		= "Allo DigiOne",
	.stream_name	= "Allo DigiOne HiFi",
	.cpu_dai_name	= "bcm2708-i2s.0",
	.codec_dai_name	= "wm8804-spdif",
	.platform_name	= "bcm2708-i2s.0",
	.codec_name	= "wm8804.1-003b",
	.dai_fmt	= SND_SOC_DAIFMT_I2S |
			  SND_SOC_DAIFMT_NB_NF |
			  SND_SOC_DAIFMT_CBM_CFM,
	.ops		= &snd_allo_digione_ops,
	.init		= snd_allo_digione_init,
},
};

/* audio machine driver */
static struct snd_soc_card snd_allo_digione = {
	.name         = "snd_allo_digione",
	.driver_name  = "AlloDigiOne",
	.owner        = THIS_MODULE,
	.dai_link     = snd_allo_digione_dai,
	.num_links    = ARRAY_SIZE(snd_allo_digione_dai),
};

static int snd_allo_digione_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_allo_digione.dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &snd_allo_digione_dai[0];

		i2s_node = of_parse_phandle(pdev->dev.of_node,
				"i2s-controller", 0);

		if (i2s_node) {
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}

		snd_allo_clk44gpio =
			devm_gpiod_get(&pdev->dev, "clock44", GPIOD_OUT_LOW);
		if (IS_ERR(snd_allo_clk44gpio))
			dev_err(&pdev->dev, "devm_gpiod_get() failed\n");

		snd_allo_clk48gpio =
			devm_gpiod_get(&pdev->dev, "clock48", GPIOD_OUT_LOW);
		if (IS_ERR(snd_allo_clk48gpio))
			dev_err(&pdev->dev, "devm_gpiod_get() failed\n");
	}

	ret = snd_soc_register_card(&snd_allo_digione);
	if (ret && ret != -EPROBE_DEFER)
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n",
			ret);

	return ret;
}

static int snd_allo_digione_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_allo_digione);
}

static const struct of_device_id snd_allo_digione_of_match[] = {
	{ .compatible = "allo,allo-digione", },
	{},
};
MODULE_DEVICE_TABLE(of, snd_allo_digione_of_match);

static struct platform_driver snd_allo_digione_driver = {
	.driver = {
		.name		= "snd-allo-digione",
		.owner		= THIS_MODULE,
		.of_match_table	= snd_allo_digione_of_match,
	},
	.probe  = snd_allo_digione_probe,
	.remove = snd_allo_digione_remove,
};

module_platform_driver(snd_allo_digione_driver);

MODULE_AUTHOR("Baswaraj <jaikumar@cem-solutions.net>");
MODULE_DESCRIPTION("ASoC Driver for Allo DigiOne");
MODULE_LICENSE("GPL v2");
