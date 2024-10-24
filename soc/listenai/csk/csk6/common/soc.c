#include <zephyr/init.h>
#include <zephyr/arch/cpu.h>

#include "soc.h"

static int csk6_init(void)
{
	/* select uart0 clk source to 24Mhz xtal */
	*((uint32_t *)(0x46000000 + 0x03C)) &= ~(1 << 16);

	/* set uart0 clk div */
	/* disable */
	*((uint32_t *)(0x46000000 + 0x03C)) &= ~(1 << 26);

	/* clear and set div n, n = 0 */
	*((uint32_t *)(0x46000000 + 0x03C)) &= ~(0x07 << 23);
	*((uint32_t *)(0x46000000 + 0x03C)) |= (0x01 << 23);

	/* clear and set div M = 1 */
	*((uint32_t *)(0x46000000 + 0x03C)) &= ~(0x07 << 20);
	*((uint32_t *)(0x46000000 + 0x03C)) |= (0x00 << 20);

	/* enable */
	*((uint32_t *)(0x46000000 + 0x03C)) |= (1 << 26);

	/* enable uart0 clk */
	*((uint32_t *)(0x46000000 + 0x03C)) |= 1 << 19;

	// volatile uint32_t *volatile pin_base = NULL;

	// /* uart0 tx pin config PA3*/
	// uint32_t pin = 3;
	// uint32_t func = 2;
	// pin_base = (volatile uint32_t *volatile)(0x46200000 + 0x00);
	// pin_base += pin;
	// *(pin_base) &= (~0x1f);
	// *(pin_base) |= (func & 0x1f);

	// /* uart0 rx pin config PA2 */
	// pin = 2;
	// func = 2;
	// pin_base = (volatile uint32_t *volatile)(0x46200000 + 0x00);
	// pin_base += pin;
	// *(pin_base) &= (~0x1f);
	// *(pin_base) |= (func & 0x1f);

	return 0;
}

void z_arm_platform_init(void)
{
}

SYS_INIT(csk6_init, PRE_KERNEL_1, 0);
