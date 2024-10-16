/*
 * Copyright (c) 2024 Fujie Li <cexianl@gmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <soc.h>

void z_arm_platform_init(void)
{

}

static int listenai_csk6x_soc_init(void)
{
	return 0;
}

SYS_INIT(listenai_csk6x_soc_init, PRE_KERNEL_1, 0);
