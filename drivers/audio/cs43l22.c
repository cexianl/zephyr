/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT cirrus_cs43l22

#include <errno.h>

#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/util.h>

#define LOG_LEVEL CONFIG_AUDIO_CODEC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs43l22);

#define CODEC_CHIP_ID 0x1C

struct reg_addr {
	uint8_t page;	  /* page number */
	uint8_t reg_addr; /* register address */
};

struct codec_driver_data {
	struct reg_addr reg_addr_cache;
};

struct codec_driver_config {
	struct i2c_dt_spec bus;
	struct gpio_dt_spec reset_gpio;
};

static struct codec_driver_data codec_device_data;
static struct codec_driver_config codec_device_config = {
	.bus = I2C_DT_SPEC_INST_GET(0),
	.reset_gpio = GPIO_DT_SPEC_INST_GET(0, reset_gpios),
};

static void codec_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	struct codec_driver_data *const dev_data = dev->data;
	const struct codec_driver_config *const dev_cfg = dev->config;

	int r = i2c_reg_write_byte_dt(&dev_cfg->bus, reg, val);
	if (r != 0) {
		LOG_ERR("write reg:%x failed, ret:%d", reg, r);
	}
}

static void codec_read_reg(const struct device *dev, uint8_t reg, uint8_t *val)
{
	struct codec_driver_data *const dev_data = dev->data;
	const struct codec_driver_config *const dev_cfg = dev->config;

	int r = i2c_reg_read_byte_dt(&dev_cfg->bus, reg, val);
	if (r != 0) {
		LOG_ERR("read reg:%x failed, ret:%d", reg, r);
	}
}

#define VOLUME_CONVERT(vol) ((vol) > 0xE6 ? (vol)-0xE7 : (vol) + 0x19)

static inline void cs43l22_passthrough_volume_set(const struct device *dev, uint8_t vol)
{
	codec_write_reg(dev, 0x14, VOLUME_CONVERT(vol));
	codec_write_reg(dev, 0x15, VOLUME_CONVERT(vol));
}

static inline void cs43l22_master_volume_set(const struct device *dev, uint8_t vol)
{
	codec_write_reg(dev, 0x20, VOLUME_CONVERT(vol));
	codec_write_reg(dev, 0x21, VOLUME_CONVERT(vol));
}

static inline void cs43l22_headphone_volume_set(const struct device *dev, uint8_t vol)
{
	codec_write_reg(dev, 0x22, VOLUME_CONVERT(vol));
	codec_write_reg(dev, 0x23, VOLUME_CONVERT(vol));
}

static inline void cs43l22_speaker_volume_set(const struct device *dev, uint8_t vol)
{
	codec_write_reg(dev, 0x24, VOLUME_CONVERT(vol));
	codec_write_reg(dev, 0x25, VOLUME_CONVERT(vol));
}

enum {
	CS43L22_OUTPUT_SEL_SPEAKER,
	CS43L22_OUTPUT_SEL_HEADPHONE,
	CS43L22_OUTPUT_SEL_BOTH,
	CS43L22_OUTPUT_SEL_AUTO,
};

#define CS43L22_REG_ID		      0x01
#define CS43L22_REG_POWER_CTL1	      0x02
#define CS43L22_REG_POWER_CTL2	      0x04
#define CS43L22_REG_CLOCKING_CTL      0x05
#define CS43L22_REG_INTERFACE_CTL1    0x06
#define CS43L22_REG_INTERFACE_CTL2    0x07
#define CS43L22_REG_PASSTHR_A_SELECT  0x08
#define CS43L22_REG_PASSTHR_B_SELECT  0x09
#define CS43L22_REG_ANALOG_ZC_SR_SETT 0x0A
#define CS43L22_REG_PASSTHR_GANG_CTL  0x0C
#define CS43L22_REG_PLAYBACK_CTL1     0x0D
#define CS43L22_REG_MISC_CTL	      0x0E
#define CS43L22_REG_PLAYBACK_CTL2     0x0F
#define CS43L22_REG_PASSTHR_A_VOL     0x14
#define CS43L22_REG_PASSTHR_B_VOL     0x15
#define CS43L22_REG_PCMA_VOL	      0x1A
#define CS43L22_REG_PCMB_VOL	      0x1B
#define CS43L22_REG_BEEP_FREQ_ON_TIME 0x1C
#define CS43L22_REG_BEEP_VOL_OFF_TIME 0x1D
#define CS43L22_REG_BEEP_TONE_CFG     0x1E
#define CS43L22_REG_TONE_CTL	      0x1F
#define CS43L22_REG_MASTER_A_VOL      0x20
#define CS43L22_REG_MASTER_B_VOL      0x21
#define CS43L22_REG_HEADPHONE_A_VOL   0x22
#define CS43L22_REG_HEADPHONE_B_VOL   0x23
#define CS43L22_REG_SPEAKER_A_VOL     0x24
#define CS43L22_REG_SPEAKER_B_VOL     0x25
#define CS43L22_REG_CH_MIXER_SWAP     0x26
#define CS43L22_REG_LIMIT_CTL1	      0x27
#define CS43L22_REG_LIMIT_CTL2	      0x28
#define CS43L22_REG_LIMIT_ATTACK_RATE 0x29
#define CS43L22_REG_OVF_CLK_STATUS    0x2E
#define CS43L22_REG_BATT_COMPENSATION 0x2F
#define CS43L22_REG_VP_BATTERY_LEVEL  0x30
#define CS43L22_REG_SPEAKER_STATUS    0x31
#define CS43L22_REG_TEMPMONITOR_CTL   0x32
#define CS43L22_REG_THERMAL_FOLDBACK  0x33
#define CS43L22_REG_CHARGE_PUMP_FREQ  0x34

static inline void cs43l22_output_dev_set(const struct device *dev, uint8_t sel)
{
	uint8_t output_dev;

	switch (sel) {
	case CS43L22_OUTPUT_SEL_SPEAKER:
		output_dev = 0xFA;
		break;
	case CS43L22_OUTPUT_SEL_HEADPHONE:
		output_dev = 0xAF;
		break;
	case CS43L22_OUTPUT_SEL_BOTH:
		output_dev = 0xAA;
		break;
	case CS43L22_OUTPUT_SEL_AUTO:
		output_dev = 0x05;
		break;
	default:
		output_dev = 0x05;
		break;
	}

	codec_write_reg(dev, CS43L22_REG_POWER_CTL2, output_dev);
}

static inline void cs43l22_volume_set(const struct device *dev, uint8_t vol)
{
	if (vol > 0xE6) {
		codec_write_reg(dev, CS43L22_REG_MASTER_A_VOL, vol - 0xE7);
		codec_write_reg(dev, CS43L22_REG_MASTER_B_VOL, vol - 0xE7);
	} else {
		codec_write_reg(dev, CS43L22_REG_MASTER_A_VOL, vol + 0x19);
		codec_write_reg(dev, CS43L22_REG_MASTER_B_VOL, vol + 0x19);
	}
}

static void cs43l22_mute_set(const struct device *dev, uint8_t mute)
{
	if (mute) {
		codec_write_reg(dev, CS43L22_REG_POWER_CTL2, 0xFF);
		codec_write_reg(dev, CS43L22_REG_HEADPHONE_A_VOL, 0x01);
		codec_write_reg(dev, CS43L22_REG_HEADPHONE_B_VOL, 0x01);
	} else {
		codec_write_reg(dev, CS43L22_REG_HEADPHONE_A_VOL, 0x00);
		codec_write_reg(dev, CS43L22_REG_HEADPHONE_B_VOL, 0x00);
		codec_write_reg(dev, CS43L22_REG_POWER_CTL2, 0x05);
	}
}

static int codec_initialize(const struct device *dev)
{
	const struct codec_driver_config *const dev_cfg = dev->config;

	if (!device_is_ready(dev_cfg->bus.bus)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(dev_cfg->reset_gpio.port)) {
		LOG_ERR("GPIO device not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&dev_cfg->reset_gpio, GPIO_OUTPUT_ACTIVE);
	k_msleep(1);
	gpio_pin_configure_dt(&dev_cfg->reset_gpio, GPIO_OUTPUT_INACTIVE);
	k_msleep(1);

	uint8_t id = 0;
	codec_read_reg(dev, CS43L22_REG_ID, &id);

	if ((id >> 3) != CODEC_CHIP_ID) {
		LOG_ERR("chip id read error, id:%02X", id);
		return -1;
	}

	/* Keep Codec powered OFF */
	codec_write_reg(dev, CS43L22_REG_POWER_CTL1, 0x01);

	cs43l22_output_dev_set(dev, CS43L22_OUTPUT_SEL_AUTO);

	codec_write_reg(dev, CS43L22_REG_CLOCKING_CTL, 0x81);
	codec_write_reg(dev, CS43L22_REG_INTERFACE_CTL1, 0x04);                     

	cs43l22_volume_set(dev, 200);

	codec_write_reg(dev, CS43L22_REG_INTERFACE_CTL1, 0x04);

	codec_write_reg(dev, CS43L22_REG_ANALOG_ZC_SR_SETT, 0x00);
	/* Disable the digital soft ramp */
	codec_write_reg(dev, CS43L22_REG_MISC_CTL, 0x04);
	/* Disable the limiter attack level */
	codec_write_reg(dev, CS43L22_REG_LIMIT_CTL1, 0x00);
	/* Adjust Bass and Treble levels */
	codec_write_reg(dev, CS43L22_REG_TONE_CTL, 0x0F);
	/* Adjust PCM volume level */
	codec_write_reg(dev, CS43L22_REG_PCMA_VOL, 0x0A);
	codec_write_reg(dev, CS43L22_REG_PCMB_VOL, 0x0A);


	/* debug only */
	codec_write_reg(dev, CS43L22_REG_MISC_CTL, 0x06);

	/* Enable Output device */
	cs43l22_mute_set(dev, false);

	/* Power on the Codec */
	codec_write_reg(dev, CS43L22_REG_POWER_CTL1, 0x9E);

	return 0;
}

static void cs43l22_start(const struct device *dev)
{
	codec_write_reg(dev, CS43L22_REG_MISC_CTL, 0x06);

	cs43l22_mute_set(dev, false);

	/* Power on the Codec */
	codec_write_reg(dev, CS43L22_REG_POWER_CTL1, 0x9E);
}

static void cs43l22_stop(const struct device *dev)
{
	/* Mute the output first */
	cs43l22_mute_set(dev, true);

	/* Disable the digital soft ramp */
	codec_write_reg(dev, CS43L22_REG_MISC_CTL, 0x04);

	/* Power down the DAC and the speaker (PMDAC and PMSPK bits)*/
	codec_write_reg(dev, CS43L22_REG_POWER_CTL1, 0x9F);
}

static const struct audio_codec_api codec_driver_api = {
	// .configure		= codec_configure,
	.start_output = cs43l22_start,
	.stop_output = cs43l22_stop,
	// .set_property		= codec_set_property,
	// .apply_properties	= codec_apply_properties,
};

DEVICE_DT_INST_DEFINE(0, codec_initialize, NULL, &codec_device_data, &codec_device_config,
		      POST_KERNEL, CONFIG_AUDIO_CODEC_INIT_PRIORITY, &codec_driver_api);
