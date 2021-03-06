/*
 * ASoC Driver for ADAU1977 ADC
 *
 * Author:	Andrey Grodzovsky <andrey2805@gmail.com>
 *		Copyright 2016
 *
 * This file is based on hifibery_dac driver by Florian Meier.
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

enum adau1977_clk_id {
    ADAU1977_SYSCLK,
};
 
enum adau1977_sysclk_src {
    ADAU1977_SYSCLK_SRC_MCLK,
    ADAU1977_SYSCLK_SRC_LRCLK,
};

static int eval_adau1977_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	
	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	return snd_soc_component_set_sysclk(codec_dai->component, ADAU1977_SYSCLK,
			ADAU1977_SYSCLK_SRC_MCLK, 11289600, SND_SOC_CLOCK_IN);
}
 
static struct snd_soc_dai_link snd_rpi_adau1977_dai[] = {
	{
	.name = "adau1977",
	.stream_name = "ADAU1977", 
	.cpu_dai_name = "bcm2708-i2s.0", 
	.codec_dai_name = "adau1977-hifi",
	.platform_name = "bcm2708-i2s.0",
	.codec_name = "adau1977.1-0011",
	.init = eval_adau1977_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM,
	},
};

/* audio machine driver */
static struct snd_soc_card snd_adau1977_adc = {
	.name         = "snd_rpi_adau1977_adc",
	.owner        = THIS_MODULE,
	.dai_link     = snd_rpi_adau1977_dai,
	.num_links    = ARRAY_SIZE(snd_rpi_adau1977_dai),
};

static int snd_adau1977_adc_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_adau1977_adc.dev = &pdev->dev;
	if (pdev->dev.of_node) {
	    struct device_node *i2s_node;
	    struct snd_soc_dai_link *dai = &snd_rpi_adau1977_dai[0];
	    i2s_node = of_parse_phandle(pdev->dev.of_node,
					"i2s-controller", 0);

	    if (i2s_node) {
		dai->cpu_dai_name = NULL;
		dai->cpu_of_node = i2s_node;
		dai->platform_name = NULL;
		dai->platform_of_node = i2s_node;
	    }
	}

	ret = snd_soc_register_card(&snd_adau1977_adc);
	if (ret && ret != -EPROBE_DEFER)
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);

	return ret;
}

static int snd_adau1977_adc_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_adau1977_adc);
}

static const struct of_device_id snd_adau1977_adc_of_match[] = {
	{ .compatible = "adi,adau1977-adc", },
	{},
};

MODULE_DEVICE_TABLE(of, snd_adau1977_adc_of_match);

static struct platform_driver snd_adau1977_adc_driver = {
        .driver = {
                .name   = "snd-adau1977-adc",
                .owner  = THIS_MODULE,
		.of_match_table = snd_adau1977_adc_of_match,
        },
        .probe          = snd_adau1977_adc_probe,
        .remove         = snd_adau1977_adc_remove,
};

module_platform_driver(snd_adau1977_adc_driver);

MODULE_AUTHOR("Andrey Grodzovsky <andrey2805@gmail.com>");
MODULE_DESCRIPTION("ASoC Driver for ADAU1977 ADC");
MODULE_LICENSE("GPL v2");
