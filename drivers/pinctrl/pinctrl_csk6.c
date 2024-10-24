/*
 * Copyright (c) 2024 ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_pinctl

#include <soc.h>
#include <zephyr/drivers/pinctrl.h>

#define PINMUX_BASE_A (0x46200000)
#define PINMUX_BASE_B (0x46200080)

#define PINMUX_AON_BASE (0x46F00000)

#define PINMUX_PAD_A 0
#define PINMUX_PAD_B 1

#define PINMUX_FUNC_MASK (0x1f)
#define PINMUX_AON_FUNC_MASK (0x1f)
#define PINMUX_AON_FUNC_EN (0x5)

static int pinctrl_csk_set(uint8_t pad, uint8_t pin, uint8_t func)
{
	volatile uint32_t *volatile pin_base = NULL;

	if (pad == PINMUX_PAD_A) {
		pin_base = (volatile uint32_t *volatile)(PINMUX_BASE_A);
	} else if (PINMUX_PAD_B) {
		pin_base = (volatile uint32_t *volatile)(PINMUX_BASE_B);
		if (pin <=5) {
			/* GPIOB0-5 in aon region */
			volatile uint32_t* volatile aon_base = (volatile uint32_t* volatile)PINMUX_AON_BASE;
            aon_base += pin;
            (*(volatile uint32_t* volatile)(aon_base)) &= (~PINMUX_FUNC_MASK);
            (*(volatile uint32_t* volatile)(aon_base)) |= PINMUX_AON_FUNC_EN;
		}
	} else {
		return -EINVAL;
	}

	pin_base += pin;
	(*(volatile uint32_t* volatile)(pin_base)) &= (~PINMUX_FUNC_MASK);
    (*(volatile uint32_t* volatile)(pin_base)) |= (func & PINMUX_FUNC_MASK);

	return 0;		
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt,
			   uintptr_t reg)
{
	ARG_UNUSED(reg);
	int i;

	for (i = 0; i < pin_cnt; i++) {
		uint8_t pad = pins[i].pinmux >> 16;
		uint8_t pin = pins[i].pinmux >> 8;
		uint8_t func = pins[i].pinmux;
		pinctrl_csk_set(pad, pin, func);
	}

	return 0;
}
