/******************************************************************************
  Copyright (C), 2019, Shenzhen G&T Industrial Development Co., Ltd
  File:    fibo_nau88c22.c
  Author:  frank.lee      
  Version: 1.0        
  Date:  2019.08
  
  Description: audio codec nau88c22

** History:
**Author (core ID)                Date               Description of Changes
**-----------------------------------------------------------------------------
** frank.lee                    26-08-2019          add for use codec nau88c22
** frank.lee                    16-09-2019          add for codec auto detect                                                          
** -----------------------------------------------------------------------------
******************************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "fibo_nau88c22.h"

#define NAU_PLL_FREQ_MAX 100000000
#define NAU_PLL_FREQ_MIN 90000000
#define NAU_PLL_REF_MAX 33000000
#define NAU_PLL_REF_MIN 8000000
#define NAU_PLL_OPTOP_MIN 6

static int g_codec_active=0; //codec exist flag
static struct snd_soc_component *nau88c22_codec;
static const int nau88c22_mclk_scaler[] = { 10, 15, 20, 30, 40, 60, 80, 120 };

static const struct reg_default nau88c22_reg_defaults[] = {
	{ NAU88C22_REG_POWER1, 0x0000 },
	{ NAU88C22_REG_POWER2, 0x0000 },
	{ NAU88C22_REG_POWER3, 0x0000 },
	{ NAU88C22_REG_IFACE, 0x0050 },
	{ NAU88C22_REG_COMP, 0x0000 },
	{ NAU88C22_REG_CLOCK, 0x0140 },
	{ NAU88C22_REG_SMPLR, 0x0000 },
	{ NAU88C22_REG_DAC, 0x0000 },
	{ NAU88C22_REG_DACGAIN, 0x00FF },
	{ NAU88C22_REG_ADC, 0x0100 },
	{ NAU88C22_REG_ADCGAIN, 0x00FF },
	{ NAU88C22_REG_EQ1, 0x012C },
	{ NAU88C22_REG_EQ2, 0x002C },
	{ NAU88C22_REG_EQ3, 0x002C },
	{ NAU88C22_REG_EQ4, 0x002C },
	{ NAU88C22_REG_EQ5, 0x002C },
	{ NAU88C22_REG_DACLIM1, 0x0032 },
	{ NAU88C22_REG_DACLIM2, 0x0000 },
	{ NAU88C22_REG_NOTCH1, 0x0000 },
	{ NAU88C22_REG_NOTCH2, 0x0000 },
	{ NAU88C22_REG_NOTCH3, 0x0000 },
	{ NAU88C22_REG_NOTCH4, 0x0000 },
	{ NAU88C22_REG_ALC1, 0x0038 },
	{ NAU88C22_REG_ALC2, 0x000B },
	{ NAU88C22_REG_ALC3, 0x0032 },
	{ NAU88C22_REG_NOISEGATE, 0x0000 },
	{ NAU88C22_REG_PLLN, 0x0008 },
	{ NAU88C22_REG_PLLK1, 0x000C },
	{ NAU88C22_REG_PLLK2, 0x0093 },
	{ NAU88C22_REG_PLLK3, 0x00E9 },
	{ NAU88C22_REG_ATTEN, 0x0000 },
	{ NAU88C22_REG_INPUT_SIGNAL, 0x0003 },
	{ NAU88C22_REG_PGAGAIN, 0x0010 },
	{ NAU88C22_REG_ADCBOOST, 0x0100 },
	{ NAU88C22_REG_OUTPUT, 0x0002 },
	{ NAU88C22_REG_SPKMIX, 0x0001 },
	{ NAU88C22_REG_SPKGAIN, 0x0039 },
	{ NAU88C22_REG_MONOMIX, 0x0001 },
	{ NAU88C22_REG_POWER4, 0x0000 },
	{ NAU88C22_REG_TSLOTCTL1, 0x0000 },
	{ NAU88C22_REG_TSLOTCTL2, 0x0020 },
	{ NAU88C22_REG_DEVICE_REVID, 0x0000 },
	{ NAU88C22_REG_I2C_DEVICEID, 0x001A },
	{ NAU88C22_REG_ADDITIONID, 0x00CA },
	{ NAU88C22_REG_RESERVE, 0x0124 },
	{ NAU88C22_REG_OUTCTL, 0x0001 },
	{ NAU88C22_REG_ALC1ENHAN1, 0x0010 },
	{ NAU88C22_REG_ALC1ENHAN2, 0x0000 },
	{ NAU88C22_REG_MISCCTL, 0x0000 },
	{ NAU88C22_REG_OUTTIEOFF, 0x0000 },
	{ NAU88C22_REG_AGCP2POUT, 0x0000 },
	{ NAU88C22_REG_AGCPOUT, 0x0000 },
	{ NAU88C22_REG_AMTCTL, 0x0000 },
	{ NAU88C22_REG_OUTTIEOFFMAN, 0x0000 },
};

//2019-08-26 add start by frank.lee@fibocom.com for add init register
static const struct reg_default Set_Codec_Reg_Init[]={
    {NAU88C22_REG_POWER1,         0x00ff},  //  1    
    {NAU88C22_REG_POWER2,         0x0015},  //  2  
    {NAU88C22_REG_POWER3,         0x00ff},  //  3  
    {NAU88C22_REG_IFACE,          0x0010},  //  4 
    {NAU88C22_REG_COMP,           0x0000},  //  5   
    //{NAU88C22_REG_CLOCK,          0x0000},  //  6    
    {0x08,                        0x0006},  //  a   
    {NAU88C22_REG_DAC,            0x0008},  //  a   
    {NAU88C22_REG_INPUT_SIGNAL,   0x0003},  //  2c
};
//2019-08-26 add end by frank.lee@fibocom.com for add init register

#define SET_CODEC_REG_INIT_NUM  ARRAY_SIZE(Set_Codec_Reg_Init)

static bool nau88c22_readable_reg(struct device *dev, unsigned int reg)
{
    return true;
    #if 0
    switch (reg) {
	case NAU88C22_REG_RESET ... NAU88C22_REG_SMPLR:
	case NAU88C22_REG_DAC ... NAU88C22_REG_DACGAIN:
	case NAU88C22_REG_ADC ... NAU88C22_REG_ADCGAIN:
	case NAU88C22_REG_EQ1 ... NAU88C22_REG_EQ5:
	case NAU88C22_REG_DACLIM1 ... NAU88C22_REG_DACLIM2:
	case NAU88C22_REG_NOTCH1 ... NAU88C22_REG_NOTCH4:
	case NAU88C22_REG_ALC1 ... NAU88C22_REG_ATTEN:
	case NAU88C22_REG_INPUT_SIGNAL ... NAU88C22_REG_PGAGAIN:
	case NAU88C22_REG_ADCBOOST:
	case NAU88C22_REG_OUTPUT ... NAU88C22_REG_SPKMIX:
	case NAU88C22_REG_SPKGAIN:
	case NAU88C22_REG_MONOMIX:
	case NAU88C22_REG_POWER4 ... NAU88C22_REG_TSLOTCTL2:
	case NAU88C22_REG_DEVICE_REVID ... NAU88C22_REG_RESERVE:
	case NAU88C22_REG_OUTCTL ... NAU88C22_REG_ALC1ENHAN2:
	case NAU88C22_REG_MISCCTL:
	case NAU88C22_REG_OUTTIEOFF ... NAU88C22_REG_OUTTIEOFFMAN:
		return true;
	default:
		return false;
	}
    #endif
}

static bool nau88c22_writeable_reg(struct device *dev, unsigned int reg)
{
    return true;
    #if 0
    switch (reg) {
	case NAU88C22_REG_RESET ... NAU88C22_REG_SMPLR:
	case NAU88C22_REG_DAC ... NAU88C22_REG_DACGAIN:
	case NAU88C22_REG_ADC ... NAU88C22_REG_ADCGAIN:
	case NAU88C22_REG_EQ1 ... NAU88C22_REG_EQ5:
	case NAU88C22_REG_DACLIM1 ... NAU88C22_REG_DACLIM2:
	case NAU88C22_REG_NOTCH1 ... NAU88C22_REG_NOTCH4:
	case NAU88C22_REG_ALC1 ... NAU88C22_REG_ATTEN:
	case NAU88C22_REG_INPUT_SIGNAL ... NAU88C22_REG_PGAGAIN:
	case NAU88C22_REG_ADCBOOST:
	case NAU88C22_REG_OUTPUT ... NAU88C22_REG_SPKMIX:
	case NAU88C22_REG_SPKGAIN:
	case NAU88C22_REG_MONOMIX:
	case NAU88C22_REG_POWER4 ... NAU88C22_REG_TSLOTCTL2:
	case NAU88C22_REG_OUTCTL ... NAU88C22_REG_ALC1ENHAN2:
	case NAU88C22_REG_MISCCTL:
	case NAU88C22_REG_OUTTIEOFF ... NAU88C22_REG_OUTTIEOFFMAN:
		return true;
	default:
		return false;
	}
    #endif
}

static bool nau88c22_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU88C22_REG_RESET:
	case NAU88C22_REG_DEVICE_REVID ... NAU88C22_REG_RESERVE:
		return true;
	default:
		return false;
	}
}

/* The EQ parameters get function is to get the 5 band equalizer control.
 * The regmap raw read can't work here because regmap doesn't provide
 * value format for value width of 9 bits. Therefore, the driver reads data
 * from cache and makes value format according to the endianness of
 * bytes type control element.
 */
static int nau88c22_eq_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	int i, reg, reg_val;
	u16 *val;

	val = (u16 *)ucontrol->value.bytes.data;
	reg = NAU88C22_REG_EQ1;
	for (i = 0; i < params->max / sizeof(u16); i++) {
		regmap_read(nau88c22->regmap, reg + i, &reg_val);
		/* conversion of 16-bit integers between native CPU format
		 * and big endian format
		 */
		reg_val = cpu_to_be16(reg_val);
		memcpy(val + i, &reg_val, sizeof(reg_val));
	}

	return 0;
}

/* The EQ parameters put function is to make configuration of 5 band equalizer
 * control. These configuration includes central frequency, equalizer gain,
 * cut-off frequency, bandwidth control, and equalizer path.
 * The regmap raw write can't work here because regmap doesn't provide
 * register and value format for register with address 7 bits and value 9 bits.
 * Therefore, the driver makes value format according to the endianness of
 * bytes type control element and writes data to codec.
 */
static int nau88c22_eq_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	u16 *val, value;
	int i, reg, ret;
	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u16 *)data;
	reg = NAU88C22_REG_EQ1;
	for (i = 0; i < params->max / sizeof(u16); i++) {
		/* conversion of 16-bit integers between native CPU format
		 * and big endian format
		 */
		value = be16_to_cpu(*(val + i));
		ret = regmap_write(nau88c22->regmap, reg + i, value);
		if (ret) {
			dev_err(component->dev, "EQ configuration fail, register: %x ret: %d\n",
				reg + i, ret);
			kfree(data);
			return ret;
		}
	}
	kfree(data);

	return 0;
}

static const char * const nau88c22_companding[] = {
	"Off", "NC", "u-law", "A-law" };

static const struct soc_enum nau88c22_companding_adc_enum =
	SOC_ENUM_SINGLE(NAU88C22_REG_COMP, NAU88C22_ADCCM_SFT,
		ARRAY_SIZE(nau88c22_companding), nau88c22_companding);

static const struct soc_enum nau88c22_companding_dac_enum =
	SOC_ENUM_SINGLE(NAU88C22_REG_COMP, NAU88C22_DACCM_SFT,
		ARRAY_SIZE(nau88c22_companding), nau88c22_companding);

static const char * const nau88c22_deemp[] = {
	"None", "32kHz", "44.1kHz", "48kHz" };

static const struct soc_enum nau88c22_deemp_enum =
	SOC_ENUM_SINGLE(NAU88C22_REG_DAC, NAU88C22_DEEMP_SFT,
		ARRAY_SIZE(nau88c22_deemp), nau88c22_deemp);

static const char * const nau88c22_eqmode[] = {"Capture", "Playback" };

static const struct soc_enum nau88c22_eqmode_enum =
	SOC_ENUM_SINGLE(NAU88C22_REG_EQ1, NAU88C22_EQM_SFT,
		ARRAY_SIZE(nau88c22_eqmode), nau88c22_eqmode);

static const char * const nau88c22_alc[] = {"Normal", "Limiter" };

static const struct soc_enum nau88c22_alc_enum =
	SOC_ENUM_SINGLE(NAU88C22_REG_ALC3, NAU88C22_ALCM_SFT,
		ARRAY_SIZE(nau88c22_alc), nau88c22_alc);

static const DECLARE_TLV_DB_SCALE(digital_tlv, -12750, 50, 1);
static const DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static const DECLARE_TLV_DB_SCALE(inpga_tlv, -1200, 75, 0);
static const DECLARE_TLV_DB_SCALE(spk_tlv, -5700, 100, 0);

static const struct snd_kcontrol_new nau88c22_snd_controls[] = {
	SOC_ENUM("ADC Companding", nau88c22_companding_adc_enum),
	SOC_ENUM("DAC Companding", nau88c22_companding_dac_enum),
	SOC_ENUM("DAC De-emphasis", nau88c22_deemp_enum),

	SOC_ENUM("EQ Function", nau88c22_eqmode_enum),
	SND_SOC_BYTES_EXT("EQ Parameters", 10,
		  nau88c22_eq_get, nau88c22_eq_put),

	SOC_SINGLE("DAC Inversion Switch", NAU88C22_REG_DAC,
		NAU88C22_DACPL_SFT, 1, 0),
//2019-09-28 add start by frank.lee@fibocom.com for modify play volume
	//SOC_SINGLE_TLV("Playback Volume", NAU88C22_REG_DACGAIN,
		//NAU88C22_DACGAIN_SFT, 0xff, 0, digital_tlv),
	SOC_DOUBLE_R_TLV("Playback Volume",
		NAU88C22_REG_DACGAIN,NAU88C22_REG_DACGAIN_RIGHT,
		0, 255, 0, digital_tlv),
		
    SOC_DOUBLE("Speak Mute",
		NAU88C22_REG_POWER3,5, 6, 1, 0),

    SOC_DOUBLE("Aux Mute",
		NAU88C22_REG_POWER3,7,8, 1, 0),
//2019-09-28 add end by frank.lee@fibocom.com for modify play volume

	SOC_SINGLE("High Pass Filter Switch", NAU88C22_REG_ADC,
		NAU88C22_HPFEN_SFT, 1, 0),
	SOC_SINGLE("High Pass Cut Off", NAU88C22_REG_ADC,
		NAU88C22_HPF_SFT, 0x7, 0),

	SOC_SINGLE("ADC Inversion Switch", NAU88C22_REG_ADC,
		NAU88C22_ADCPL_SFT, 1, 0),
	SOC_SINGLE_TLV("Capture Volume", NAU88C22_REG_ADCGAIN,
		NAU88C22_ADCGAIN_SFT, 0xff, 0, digital_tlv),

	SOC_SINGLE_TLV("EQ1 Volume", NAU88C22_REG_EQ1,
		NAU88C22_EQ1GC_SFT, 0x18, 1, eq_tlv),
	SOC_SINGLE_TLV("EQ2 Volume", NAU88C22_REG_EQ2,
		NAU88C22_EQ2GC_SFT, 0x18, 1, eq_tlv),
	SOC_SINGLE_TLV("EQ3 Volume", NAU88C22_REG_EQ3,
		NAU88C22_EQ3GC_SFT, 0x18, 1, eq_tlv),
	SOC_SINGLE_TLV("EQ4 Volume", NAU88C22_REG_EQ4,
		NAU88C22_EQ4GC_SFT, 0x18, 1, eq_tlv),
	SOC_SINGLE_TLV("EQ5 Volume", NAU88C22_REG_EQ5,
		NAU88C22_EQ5GC_SFT, 0x18, 1, eq_tlv),

	SOC_SINGLE("DAC Limiter Switch", NAU88C22_REG_DACLIM1,
		NAU88C22_DACLIMEN_SFT, 1, 0),
	SOC_SINGLE("DAC Limiter Decay", NAU88C22_REG_DACLIM1,
		NAU88C22_DACLIMDCY_SFT, 0xf, 0),
	SOC_SINGLE("DAC Limiter Attack", NAU88C22_REG_DACLIM1,
		NAU88C22_DACLIMATK_SFT, 0xf, 0),
	SOC_SINGLE("DAC Limiter Threshold", NAU88C22_REG_DACLIM2,
		NAU88C22_DACLIMTHL_SFT, 0x7, 0),
	SOC_SINGLE("DAC Limiter Boost", NAU88C22_REG_DACLIM2,
		NAU88C22_DACLIMBST_SFT, 0xf, 0),

	SOC_ENUM("ALC Mode", nau88c22_alc_enum),
	SOC_SINGLE("ALC Enable Switch", NAU88C22_REG_ALC1,
		NAU88C22_ALCEN_SFT, 1, 0),
	SOC_SINGLE("ALC Max Volume", NAU88C22_REG_ALC1,
		NAU88C22_ALCMXGAIN_SFT, 0x7, 0),
	SOC_SINGLE("ALC Min Volume", NAU88C22_REG_ALC1,
		NAU88C22_ALCMINGAIN_SFT, 0x7, 0),
	SOC_SINGLE("ALC ZC Switch", NAU88C22_REG_ALC2,
		NAU88C22_ALCZC_SFT, 1, 0),
	SOC_SINGLE("ALC Hold", NAU88C22_REG_ALC2,
		NAU88C22_ALCHT_SFT, 0xf, 0),
	SOC_SINGLE("ALC Target", NAU88C22_REG_ALC2,
		NAU88C22_ALCSL_SFT, 0xf, 0),
	SOC_SINGLE("ALC Decay", NAU88C22_REG_ALC3,
		NAU88C22_ALCDCY_SFT, 0xf, 0),
	SOC_SINGLE("ALC Attack", NAU88C22_REG_ALC3,
		NAU88C22_ALCATK_SFT, 0xf, 0),
	SOC_SINGLE("ALC Noise Gate Switch", NAU88C22_REG_NOISEGATE,
		NAU88C22_ALCNEN_SFT, 1, 0),
	SOC_SINGLE("ALC Noise Gate Threshold", NAU88C22_REG_NOISEGATE,
		NAU88C22_ALCNTH_SFT, 0x7, 0),

	SOC_SINGLE("PGA ZC Switch", NAU88C22_REG_PGAGAIN,
		NAU88C22_PGAZC_SFT, 1, 0),
	//SOC_SINGLE_TLV("PGA Volume", NAU88C22_REG_PGAGAIN,
		//NAU88C22_PGAGAIN_SFT, 0x3f, 0, inpga_tlv),
		
	/* Input PGA volume */
	SOC_DOUBLE_R_TLV("Input PGA Volume",
		NAU88C22_REG_PGAGAIN,NAU88C22_REG_PGAGAIN_RIGHT,
		0, 63, 0, inpga_tlv),

	SOC_SINGLE("Speaker ZC Switch", NAU88C22_REG_SPKGAIN,
		NAU88C22_SPKZC_SFT, 1, 0),
	SOC_SINGLE("Speaker Mute Switch", NAU88C22_REG_SPKGAIN,
		NAU88C22_SPKMT_SFT, 1, 0),
	SOC_SINGLE_TLV("Speaker Volume", NAU88C22_REG_SPKGAIN,
		NAU88C22_SPKGAIN_SFT, 0x3f, 0, spk_tlv),

	SOC_SINGLE("Capture Boost(+20dB)", NAU88C22_REG_ADCBOOST,
		NAU88C22_PGABST_SFT, 1, 0),
	SOC_SINGLE("Mono Mute Switch", NAU88C22_REG_MONOMIX,
		NAU88C22_MOUTMXMT_SFT, 1, 0),

	SOC_SINGLE("DAC Oversampling Rate(128x) Switch", NAU88C22_REG_DAC,
		NAU88C22_DACOS_SFT, 1, 0),
	SOC_SINGLE("ADC Oversampling Rate(128x) Switch", NAU88C22_REG_ADC,
		NAU88C22_ADCOS_SFT, 1, 0),
};
//2019-08-29 add start by frank.lee for debug
#if 1
/* Speaker Output Mixer */
static const struct snd_kcontrol_new nau88c22_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Bypass Switch", NAU88C22_REG_SPKMIX,
		NAU88C22_BYPSPK_SFT, 1, 0),
	SOC_DAPM_SINGLE("PCM Playback Switch", NAU88C22_REG_SPKMIX,
		NAU88C22_DACSPK_SFT, 1, 0),
};

/* Mono Output Mixer */
static const struct snd_kcontrol_new nau88c22_mono_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Bypass Switch", NAU88C22_REG_MONOMIX,
		NAU88C22_BYPMOUT_SFT, 1, 0),
	SOC_DAPM_SINGLE("PCM Playback Switch", NAU88C22_REG_MONOMIX,
		NAU88C22_DACMOUT_SFT, 1, 0),
};

/* PGA Mute */
static const struct snd_kcontrol_new nau88c22_inpga_mute[] = {
	SOC_DAPM_SINGLE("PGA Mute Switch", NAU88C22_REG_PGAGAIN,
		NAU88C22_PGAMT_SFT, 1, 0),
};

/* Input PGA */
static const struct snd_kcontrol_new nau88c22_inpga[] = {
	SOC_DAPM_SINGLE("MicN Switch", NAU88C22_REG_INPUT_SIGNAL,
		NAU88C22_NMICPGA_SFT, 1, 0),
	SOC_DAPM_SINGLE("MicP Switch", NAU88C22_REG_INPUT_SIGNAL,
		NAU88C22_PMICPGA_SFT, 1, 0),
};

/* Mic Input boost vol */
static const struct snd_kcontrol_new nau88c22_mic_boost_controls =
	SOC_DAPM_SINGLE("Mic Volume", NAU88C22_REG_ADCBOOST,
		NAU88C22_PMICBSTGAIN_SFT, 0x7, 0);

/* Loopback Switch */
static const struct snd_kcontrol_new nau88c22_loopback =
	SOC_DAPM_SINGLE("Switch", NAU88C22_REG_COMP,
		NAU88C22_ADDAP_SFT, 1, 0);


static int check_mclk_select_pll(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(source->dapm);
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	unsigned int value;

	regmap_read(nau88c22->regmap, NAU88C22_REG_CLOCK, &value);
    
    dev_err(component->dev, ">>>>>>check_mclk_select_pll value=%d<<<<<\n",value & NAU88C22_CLKM_MASK);
	return (value & NAU88C22_CLKM_MASK);
}


static const struct snd_soc_dapm_widget nau88c22_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Speaker Mixer", NAU88C22_REG_POWER3,
		NAU88C22_SPKMX_EN_SFT, 0, &nau88c22_speaker_mixer_controls[0],
		ARRAY_SIZE(nau88c22_speaker_mixer_controls)),
	SND_SOC_DAPM_MIXER("Mono Mixer", NAU88C22_REG_POWER3,
		NAU88C22_MOUTMX_EN_SFT, 0, &nau88c22_mono_mixer_controls[0],
		ARRAY_SIZE(nau88c22_mono_mixer_controls)),
	SND_SOC_DAPM_DAC("DAC", "HiFi Playback", NAU88C22_REG_POWER3,
		NAU88C22_DAC_EN_SFT, 0),
	SND_SOC_DAPM_ADC("ADC", "HiFi Capture", NAU88C22_REG_POWER2,
		NAU88C22_ADC_EN_SFT, 0),
	//2019-08-27 modify start by frnak.lee@fibocom.com	
	SND_SOC_DAPM_PGA("SpkN Out", NAU88C22_REG_POWER3,
		NAU88C22_NSPK_EN_SFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SpkP Out", NAU88C22_REG_POWER3,
		NAU88C22_PSPK_EN_SFT, 0, NULL, 0),
	//SND_SOC_DAPM_PGA("SpkN Out", NAU88C22_REG_POWER3,
	//	7, 0, NULL, 0),
	//SND_SOC_DAPM_PGA("SpkP Out", NAU88C22_REG_POWER3,
		//8, 0, NULL, 0),
	//2019-08-27 modify end by frnak.lee@fibocom.com	
	SND_SOC_DAPM_PGA("Mono Out", NAU88C22_REG_POWER3,
		NAU88C22_MOUT_EN_SFT, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("Input PGA", NAU88C22_REG_POWER2,
		NAU88C22_PGA_EN_SFT, 0, nau88c22_inpga,
		ARRAY_SIZE(nau88c22_inpga)),
	SND_SOC_DAPM_MIXER("Input Boost Stage", NAU88C22_REG_POWER2,
		NAU88C22_BST_EN_SFT, 0, nau88c22_inpga_mute,
		ARRAY_SIZE(nau88c22_inpga_mute)),

	SND_SOC_DAPM_SUPPLY("Mic Bias", NAU88C22_REG_POWER1,
		NAU88C22_MICBIAS_EN_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL", NAU88C22_REG_POWER1,
		NAU88C22_PLL_EN_SFT, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH("Digital Loopback", SND_SOC_NOPM, 0, 0,
		&nau88c22_loopback),

	SND_SOC_DAPM_INPUT("MICN"),
	SND_SOC_DAPM_INPUT("MICP"),
	SND_SOC_DAPM_OUTPUT("MONOOUT"),
	SND_SOC_DAPM_OUTPUT("SPKOUTP"),
	SND_SOC_DAPM_OUTPUT("SPKOUTN"),
};

static const struct snd_soc_dapm_route nau88c22_dapm_routes[] = {
	{"DAC", NULL, "PLL", check_mclk_select_pll},


	{"SPKOUTP",NULL,"Playback"},
	{"Capture",NULL,"MICP"},
	
	/* Mono output mixer */
	{"Mono Mixer", "PCM Playback Switch", "DAC"},
	{"Mono Mixer", "Line Bypass Switch", "Input Boost Stage"},

	/* Speaker output mixer */
	{"Speaker Mixer", "PCM Playback Switch", "DAC"},
	{"Speaker Mixer", "Line Bypass Switch", "Input Boost Stage"},

	/* Outputs */
	{"Mono Out", NULL, "Mono Mixer"},
	{"MONOOUT", NULL, "Mono Out"},
	{"SpkN Out", NULL, "Speaker Mixer"},
	{"SpkP Out", NULL, "Speaker Mixer"},
	{"SPKOUTN", NULL, "SpkN Out"},
	{"SPKOUTP", NULL, "SpkP Out"},

	/* Input Boost Stage */
	{"ADC", NULL, "Input Boost Stage"},
	{"ADC", NULL, "PLL", check_mclk_select_pll},
	{"Input Boost Stage", NULL, "Input PGA"},
	{"Input Boost Stage", NULL, "MICP"},

	/* Input PGA */
	{"Input PGA", NULL, "Mic Bias"},
	{"Input PGA", "MicN Switch", "MICN"},
	{"Input PGA", "MicP Switch", "MICP"},

	/* Digital Looptack */
	{"Digital Loopback", "Switch", "ADC"},
	{"DAC", NULL, "Digital Loopback"},
};

//2019-08-29 add end by frank.lee for debug
#endif
static int nau88c22_set_sysclk(struct snd_soc_dai *dai,
				 int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = dai->component;
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	nau88c22->clk_id = clk_id;
	nau88c22->sysclk = freq;
	dev_dbg(nau88c22->dev, "master sysclk %dHz, source %s\n",
		freq, clk_id == NAU88C22_SCLK_PLL ? "PLL" : "MCLK");

	return 0;
}

static int nau88l0_calc_pll(unsigned int pll_in,
	unsigned int fs, struct nau88c22_pll *pll_param)
{
	u64 f2, f2_max, pll_ratio;
	int i, scal_sel;

	if (pll_in > NAU_PLL_REF_MAX || pll_in < NAU_PLL_REF_MIN)
		return -EINVAL;

	f2_max = 0;
	scal_sel = ARRAY_SIZE(nau88c22_mclk_scaler);
	for (i = 0; i < ARRAY_SIZE(nau88c22_mclk_scaler); i++) {
		f2 = 256 * fs * 4 * nau88c22_mclk_scaler[i] / 10;
		if (f2 > NAU_PLL_FREQ_MIN && f2 < NAU_PLL_FREQ_MAX &&
			f2_max < f2) {
			f2_max = f2;
			scal_sel = i;
		}
	}
	if (ARRAY_SIZE(nau88c22_mclk_scaler) == scal_sel)
		return -EINVAL;
	pll_param->mclk_scaler = scal_sel;
	f2 = f2_max;

	/* Calculate the PLL 4-bit integer input and the PLL 24-bit fractional
	 * input; round up the 24+4bit.
	 */
	pll_ratio = div_u64(f2 << 28, pll_in);
	pll_param->pre_factor = 0;
	if (((pll_ratio >> 28) & 0xF) < NAU_PLL_OPTOP_MIN) {
		pll_ratio <<= 1;
		pll_param->pre_factor = 1;
	}
	pll_param->pll_int = (pll_ratio >> 28) & 0xF;
	pll_param->pll_frac = ((pll_ratio & 0xFFFFFFF) >> 4);

	return 0;
}

static int nau88c22_set_pll(struct snd_soc_dai *codec_dai, int pll_id,
	int source, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_component *component = codec_dai->component;
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	struct regmap *map = nau88c22->regmap;
	struct nau88c22_pll *pll_param = &nau88c22->pll;
	int ret, fs;
	fs = freq_out / 256;
	ret = nau88l0_calc_pll(freq_in, fs, pll_param);
	if (ret < 0) {
		dev_err(nau88c22->dev, "Unsupported input clock %d\n", freq_in);
		return ret;
	}
	dev_info(nau88c22->dev, "pll_int=%x pll_frac=%x mclk_scaler=%x pre_factor=%x\n",
		pll_param->pll_int, pll_param->pll_frac, pll_param->mclk_scaler,
		pll_param->pre_factor);

	regmap_update_bits(map, NAU88C22_REG_PLLN,
		NAU88C22_PLLMCLK_DIV2 | NAU88C22_PLLN_MASK,
		(pll_param->pre_factor ? NAU88C22_PLLMCLK_DIV2 : 0) |
		pll_param->pll_int);
	regmap_write(map, NAU88C22_REG_PLLK1,
		(pll_param->pll_frac >> NAU88C22_PLLK1_SFT) &
		NAU88C22_PLLK1_MASK);
	regmap_write(map, NAU88C22_REG_PLLK2,
		(pll_param->pll_frac >> NAU88C22_PLLK2_SFT) &
		NAU88C22_PLLK2_MASK);
	regmap_write(map, NAU88C22_REG_PLLK3,
		pll_param->pll_frac & NAU88C22_PLLK3_MASK);
	regmap_update_bits(map, NAU88C22_REG_CLOCK, NAU88C22_MCLKSEL_MASK,
		pll_param->mclk_scaler << NAU88C22_MCLKSEL_SFT);
	regmap_update_bits(map, NAU88C22_REG_CLOCK,
		NAU88C22_CLKM_MASK, NAU88C22_CLKM_PLL);

	return 0;
}

static int nau88c22_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	u16 ctrl1_val = 0, ctrl2_val = 0;
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ctrl2_val |= NAU88C22_CLKIO_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		ctrl1_val |= NAU88C22_AIFMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ctrl1_val |= NAU88C22_AIFMT_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		ctrl1_val |= NAU88C22_AIFMT_PCM_A;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		ctrl1_val |= NAU88C22_BCLKP_IB | NAU88C22_FSP_IF;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		ctrl1_val |= NAU88C22_BCLKP_IB;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		ctrl1_val |= NAU88C22_FSP_IF;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(nau88c22->regmap, NAU88C22_REG_IFACE,
		NAU88C22_AIFMT_MASK | NAU88C22_FSP_IF |
		NAU88C22_BCLKP_IB, ctrl1_val);
	regmap_update_bits(nau88c22->regmap, NAU88C22_REG_CLOCK,
		NAU88C22_CLKIO_MASK, ctrl2_val);

	return 0;
}
//2019-08-26 modify start by frank.lee@fibocom.com for mask mclk
#if 0
static int nau88c22_mclk_clkdiv(struct nau88c22 *nau88c22, int rate)
{
	int i, sclk, imclk = rate * 256, div = 0;

	if (!nau88c22->sysclk) {
		dev_err(nau88c22->dev, "Make mclk div configuration fail because of invalid system clock\n");
		return -EINVAL;
	}

	/* Configure the master clock prescaler div to make system
	 * clock to approximate the internal master clock (IMCLK);
	 * and large or equal to IMCLK.
	 */
	for (i = 1; i < ARRAY_SIZE(nau88c22_mclk_scaler); i++) {
		sclk = (nau88c22->sysclk * 10) /
			nau88c22_mclk_scaler[i];
		if (sclk < imclk)
			break;
		div = i;
	}
	dev_dbg(nau88c22->dev,
		"master clock prescaler %x for fs %d\n", div, rate);

	/* master clock from MCLK and disable PLL */
	regmap_update_bits(nau88c22->regmap, NAU88C22_REG_CLOCK,
		NAU88C22_MCLKSEL_MASK, (div << NAU88C22_MCLKSEL_SFT));
	regmap_update_bits(nau88c22->regmap, NAU88C22_REG_CLOCK,
		NAU88C22_CLKM_MASK, NAU88C22_CLKM_MCLK);

	return 0;
}
#endif
//2019-08-26 modify end by frank.lee@fibocom.com for mask mclk

static int nau88c22_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	int val_len = 0, val_rate = 0, ret = 0;
	switch (params_width(params)) {
	case 16:
		break;
	case 20:
		val_len |= NAU88C22_WLEN_20;
		break;
	case 24:
		val_len |= NAU88C22_WLEN_24;
		break;
	case 32:
		val_len |= NAU88C22_WLEN_32;
		break;
	}

	switch (params_rate(params)) {
	case 8000:
		val_rate |= NAU88C22_SMPLR_8K;
		break;
	case 11025:
		val_rate |= NAU88C22_SMPLR_12K;
		break;
	case 16000:
		val_rate |= NAU88C22_SMPLR_16K;
		break;
	case 22050:
		val_rate |= NAU88C22_SMPLR_24K;
		break;
	case 32000:
		val_rate |= NAU88C22_SMPLR_32K;
		break;
	case 44100:
	case 48000:
		break;
	}

	regmap_update_bits(nau88c22->regmap, NAU88C22_REG_IFACE,
		NAU88C22_WLEN_MASK, val_len);
	regmap_update_bits(nau88c22->regmap, NAU88C22_REG_SMPLR,
		NAU88C22_SMPLR_MASK, val_rate);
//2019-08-26 modify start by frank.lee@fibocom.com for mask codec master clock
#if 0
	/* If the master clock is from MCLK, provide the runtime FS for driver
	 * to get the master clock prescaler configuration.
	 */
	if (nau88c22->clk_id == NAU88C22_SCLK_MCLK) {
		ret = nau88c22_mclk_clkdiv(nau88c22, params_rate(params));
		if (ret < 0)
			dev_err(nau88c22->dev, "MCLK div configuration fail\n");
	}
#endif
//2019-08-26 modify end by frank.lee@fibocom.com for mask codec master clock
	return ret;
}

#if 0
static int nau88c22_set_bias_level(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	struct regmap *map = nau88c22->regmap;
	
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		regmap_update_bits(map, NAU88C22_REG_POWER1,
			NAU88C22_REFIMP_MASK, NAU88C22_REFIMP_80K);
		break;

	case SND_SOC_BIAS_STANDBY:
		regmap_update_bits(map, NAU88C22_REG_POWER1,
			NAU88C22_IOBUF_EN | NAU88C22_ABIAS_EN,
			NAU88C22_IOBUF_EN | NAU88C22_ABIAS_EN);

		if (snd_soc_dapm_get_bias_level(component) == SND_SOC_BIAS_OFF) {
			regcache_sync(map);
			regmap_update_bits(map, NAU88C22_REG_POWER1,
				NAU88C22_REFIMP_MASK, NAU88C22_REFIMP_3K);
			mdelay(100);
		}
		regmap_update_bits(map, NAU88C22_REG_POWER1,
			NAU88C22_REFIMP_MASK, NAU88C22_REFIMP_300K);
		break;

	case SND_SOC_BIAS_OFF:
		//regmap_write(map, NAU88C22_REG_POWER1, 0);
		//regmap_write(map, NAU88C22_REG_POWER2, 0);
		//regmap_write(map, NAU88C22_REG_POWER3, 0);
		break;
	}

	return 0;
}
#else
static int nau88c22_set_bias_level(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	struct regmap *map = nau88c22->regmap;
	
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		regmap_update_bits(map, NAU88C22_REG_POWER1,
			NAU88C22_REFIMP_MASK, NAU88C22_REFIMP_80K);
		break;

	case SND_SOC_BIAS_STANDBY:
		regmap_update_bits(map, NAU88C22_REG_POWER1,
			NAU88C22_IOBUF_EN | NAU88C22_ABIAS_EN,
			NAU88C22_IOBUF_EN | NAU88C22_ABIAS_EN);

		if (snd_soc_component_get_bias_level(component) == SND_SOC_BIAS_OFF) {
			regcache_sync(map);
			regmap_update_bits(map, NAU88C22_REG_POWER1,
				NAU88C22_REFIMP_MASK, NAU88C22_REFIMP_3K);
			mdelay(100);
		}
		regmap_update_bits(map, NAU88C22_REG_POWER1,
			NAU88C22_REFIMP_MASK, NAU88C22_REFIMP_300K);
		break;

	case SND_SOC_BIAS_OFF:
		//regmap_write(map, NAU88C22_REG_POWER1, 0);
		//regmap_write(map, NAU88C22_REG_POWER2, 0);
		//regmap_write(map, NAU88C22_REG_POWER3, 0);
		break;
	}

	return 0;
}
#endif

/*
 * These registers contain an "update" bit - bit 8. This means, for example,
 * that one can write new DAC digital volume for both channels, but only when
 * the update bit is set, will also the volume be updated - simultaneously for
 * both channels.
 */
static const int update_reg[] = {
	0x0b,
	0x0c,
	0x0f,
	0x10,
	0x2d,
	0x2e,
	0x34,
	0x35,
	0x36,
	0x37,
};

//2019-08-27 add start by frank.lee for add codec probe function
static int nau88c22_codec_probe(struct snd_soc_component *component)
{
    int i = 0;
    struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
	//struct regmap *map = nau88c22->regmap;
	nau88c22_codec=component;
    //2019-09-28 add start by frank.lee@fibocom.com for add reg update
    /*
	 * Set the update bit in all registers, that have one. This way all
	 * writes to those registers will also cause the update bit to be
	 * written.
	 */
	printk(KERN_INFO "nau88c22 module being probed\n");
	for (i = 0; i < ARRAY_SIZE(update_reg); i++)
		snd_soc_component_update_bits(component, update_reg[i], 0x100, 0x100);
    //2019-09-28 add end by frank.lee@fibocom.com for add reg update
    
	//2019-08-26 add start by frank.lee@fibocom.com for add reg init
	 for(i=0;i<SET_CODEC_REG_INIT_NUM;i++) {
		regmap_write(nau88c22->regmap,Set_Codec_Reg_Init[i].reg,Set_Codec_Reg_Init[i].def);
    }
	//2019-08-26 add end by frank.lee@fibocom.com for add reg init
 
    return 0;
}
//2019-08-27 add end by frank.lee for add codec probe function

//2019-09-28 add start by frank.lee for add codec write and read function
static int nau88c22_write(struct snd_soc_component  *component, unsigned int reg,
  unsigned int value)
{
  struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
  regmap_write(nau88c22->regmap, reg, value);
  return 0;
}

static unsigned int nau88c22_read(struct snd_soc_component *component, unsigned int reg)
{
  unsigned int value=0x0;
  struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(component);
  regmap_read(nau88c22->regmap, reg, &value);
  return value;
}
//2019-09-28 add end by frank.lee for add codec write and read function


 
#define NAU88C22_RATES (SNDRV_PCM_RATE_8000_48000)

#define NAU88C22_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops nau88c22_ops = {
	.hw_params = nau88c22_pcm_hw_params,
	.set_fmt = nau88c22_set_dai_fmt,
	.set_sysclk = nau88c22_set_sysclk,
	.set_pll = nau88c22_set_pll,
};

static struct snd_soc_dai_driver nau88c22_dai = {
	.name = "nau88c22-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,   /* Only 1 channel of data */
		.rates = NAU88C22_RATES,
		.formats = NAU88C22_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,   /* Only 1 channel of data */
		.rates = NAU88C22_RATES,
		.formats = NAU88C22_FORMATS,
	},
	.ops = &nau88c22_ops,
	.symmetric_rates = 1,
};

static const struct regmap_config nau88c22_regmap_config = {
	.reg_bits = 7,
	.val_bits = 9,

	.max_register = NAU88C22_REG_MAX,
	.readable_reg = nau88c22_readable_reg,
	.writeable_reg = nau88c22_writeable_reg,
	.volatile_reg = nau88c22_volatile_reg,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = nau88c22_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(nau88c22_reg_defaults),
};

static const struct snd_soc_component_driver nau88c22_codec_driver = {
	.probe = nau88c22_codec_probe,
	.set_bias_level = nau88c22_set_bias_level,
	.suspend_bias_off = true,
	//2019-09-28 add start by frank.lee for add codec write and read function
    .read = nau88c22_read,
    .write = nau88c22_write,
    //2019-09-28 add end by frank.lee for add codec write and read function
	.controls = nau88c22_snd_controls,
	.num_controls = ARRAY_SIZE(nau88c22_snd_controls),
//2019-08-29 add start by frank.lee for debug	
	#if 1
	.dapm_widgets = nau88c22_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(nau88c22_dapm_widgets),
	.dapm_routes = nau88c22_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(nau88c22_dapm_routes),
	#endif
//2019-08-29 add end by frank.lee for debug
};

int nau88c22_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{

	
    struct device *dev = &i2c->dev;
	struct nau88c22 *nau88c22 = dev_get_platdata(dev);
	int val=0;
    unsigned char data[2]={0};

	if (!nau88c22) {
		nau88c22 = devm_kzalloc(dev, sizeof(*nau88c22), GFP_KERNEL);
		if (!nau88c22)
			return -ENOMEM;
	}	
	i2c_set_clientdata(i2c, nau88c22);
	nau88c22->regmap = devm_regmap_init_i2c(i2c, &nau88c22_regmap_config);
	if (IS_ERR(nau88c22->regmap))
		return PTR_ERR(nau88c22->regmap);	
	nau88c22->dev = dev;
    regmap_write(nau88c22->regmap, NAU88C22_REG_RESET, 0x00);
    mdelay(100);
//2019-09-12 add start by frank.lee@fibocom.com for codec auto check if exist
    data[0] = (NAU88C22_REG_I2C_DEVICEID<<1);
    if(i2c_master_send(i2c, data, 1) == 1) 
    {
        i2c_master_recv(i2c,data,2);
        val= ((data[0] & 0x01) << 8) | data[1];
    }
    if(val != 0x1a)
    {    	
        dev_err(&i2c->dev, "device is not nau88c22 ID=0x%x\n", val);
        return -EINVAL;
    }
    g_codec_active = 1;
//2019-09-12 add end by frank.lee@fibocom.com for codec auto check if exist
	return devm_snd_soc_register_component(dev,
		&nau88c22_codec_driver, &nau88c22_dai, 1);
}
EXPORT_SYMBOL(nau88c22_i2c_probe);


int nau88c22_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_component(&client->dev);

	return 0;
}
EXPORT_SYMBOL(nau88c22_i2c_remove);

#if 0
static const struct i2c_device_id nau88c22_i2c_id[] = {
	{ "nau88c22", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nau88c22_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id nau88c22_of_match[] = {
	{ .compatible = "nuvoton,nau88c22", },
	{ }
};
MODULE_DEVICE_TABLE(of, nau88c22_of_match);
#endif

static struct i2c_driver nau88c22_i2c_driver = {
	.driver = {
		.name = "nau88c22",
		.of_match_table = of_match_ptr(nau88c22_of_match),
	},
	.probe =    nau88c22_i2c_probe,
	.remove =   nau88c22_i2c_remove,
	.id_table = nau88c22_i2c_id,
};

module_i2c_driver(nau88c22_i2c_driver);
#endif
//2019-08-27 add start by frank.lee for debug
static int nau88c22_debug_reg_set(const char *buffer, const struct kernel_param *kp)
{
	int reg, val;
    if(g_codec_active)
    {
    	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(nau88c22_codec);

    	if(!buffer)
        	return -EINVAL;
    	if(sscanf(buffer, "%x,%x", (uint *)&reg, (uint *)&val) != 2) {
        	printk(KERN_ERR "invalid val = %s\n", buffer);
        	return -EINVAL;
    	}
    	pr_debug(KERN_INFO"%s:R0x%x=0x%x\n", __func__, reg, val);
	regmap_write(nau88c22->regmap,reg, val);
    }
    return 0;
}
static int nau88c22_debug_reg_get(char *buffer, const struct kernel_param *kp)
{
    int reg, val;
    int len = 0;
    if(g_codec_active)
    {
    	struct nau88c22 *nau88c22 = snd_soc_component_get_drvdata(nau88c22_codec);
    	
        len = sprintf(buffer, "nau8810 DUMP registers:");
        for(reg = 0; reg < NAU88C22_REG_MAX; reg++) {
            regmap_read(nau88c22->regmap,reg,&val);
            if((!(reg%8)) || (reg ==(NAU88C22_REG_MAX-1)))
                len += sprintf(buffer+len,"\n");
            len += sprintf(buffer+len, "R%02x:%03x ", reg, val);
        }
    }
    return len;
}

static struct kernel_param_ops reg_param_ops = {
	.set = nau88c22_debug_reg_set,
	.get = nau88c22_debug_reg_get,
};

module_param_cb(regs, &reg_param_ops,NULL, 0644);
MODULE_PARM_DESC(regs, "read and write codec regs");

//module_param_call(regs, nau88c22_debug_reg_set, nau88c22_debug_reg_get, &dummy, 0644);
//2019-08-27 add end by frank.lee for debug

MODULE_DESCRIPTION("ASoC NAU88C22 driver");
MODULE_AUTHOR("David Lin <ctlin0@nuvoton.com>");
MODULE_LICENSE("GPL v2");
