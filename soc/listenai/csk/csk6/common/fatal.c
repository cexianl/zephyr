#include <zephyr/kernel.h>
#include <kernel_arch_interface.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(csk6_fatal);

#if CONFIG_CSK6_FATAL_BACKTRACE
#define CODE_SPACE_START_ADDR   (DT_REG_ADDR(DT_CHOSEN(zephyr_flash)))
#define CODE_SPACE_SIZE         (DT_REG_SIZE(DT_CHOSEN(zephyr_flash)))


#ifndef _CALL_STACK_MAX_DEPTH
#define _CALL_STACK_MAX_DEPTH       16
#endif

/* check the disassembly instruction is 'BL' or 'BLX' */
static bool disassembly_ins_is_bl_blx(uint32_t addr) {
    uint16_t ins1 = *((uint16_t *)addr);
    uint16_t ins2 = *((uint16_t *)(addr + 2));

#define BL_INS_MASK         0xF800
#define BL_INS_HIGH         0xF800
#define BL_INS_LOW          0xF000
#define BLX_INX_MASK        0xFF00
#define BLX_INX             0x4700

    if ((ins2 & BL_INS_MASK) == BL_INS_HIGH && (ins1 & BL_INS_MASK) == BL_INS_LOW) {
        return true;
    } else if ((ins2 & BLX_INX_MASK) == BLX_INX) {
        return true;
    } else {
        return false;
    }
}

static size_t _backtrace_call_stack(uint32_t *buffer, size_t size, const struct arch_esf *esf)
{
	uint32_t stack_start_addr, stack_size, pc;
    uint32_t depth = 0;
    bool regs_saved_lr_is_valid = false;
	uint32_t sp = (uint32_t)esf->extra_info.callee->psp;

	uint32_t code_start_addr = CODE_SPACE_START_ADDR;
	uint32_t code_size = CODE_SPACE_SIZE;


	/* first depth is PC */
	buffer[depth++] = esf->basic.pc;
	/* fix the LR address in thumb mode */
	pc = esf->basic.lr - 1;
	/* fix the LR address to pc address */
	pc -= 1;
	if ((pc >= code_start_addr) && (pc <= code_start_addr + code_size) && (depth < _CALL_STACK_MAX_DEPTH)
			&& (depth < size)) {
		buffer[depth++] = pc;
		regs_saved_lr_is_valid = true;
	}


	//gets the current thread stack space
	k_tid_t thread = k_current_get();
	stack_start_addr = thread->stack_info.start;
	stack_size = thread->stack_info.size;

	/*
		when stack overflow? --Due to the stack limit of the m33 processor, this effectively 
		prevents stack pushed operations that could compromise the integrity of the stack.
	*/

    /* copy called function address */
    for (; sp < stack_start_addr + stack_size; sp += sizeof(size_t)) {
        /* the *sp value may be LR, so need decrease a word to PC */
        pc = *((uint32_t *) sp) - sizeof(size_t);
        /* the Cortex-M using thumb instruction, so the pc must be an odd number */
        if (pc % 2 == 0) {
            continue;
        }
        /* fix the PC address in thumb mode */
        pc = *((uint32_t *) sp) - 1;

        if ((pc >= code_start_addr + sizeof(size_t)) && (pc <= code_start_addr + code_size) && (depth < _CALL_STACK_MAX_DEPTH)
                /* check the the instruction before PC address is 'BL' or 'BLX' */
                && disassembly_ins_is_bl_blx(pc - sizeof(size_t)) && (depth < size)) {
			/* fix the LR address to pc address */
			pc -= 1;

            /* the second depth function may be already saved, so need ignore repeat */
            if ((depth == 2) && regs_saved_lr_is_valid && (pc == buffer[1])) {
                continue;
            }
            buffer[depth++] = pc;
        }
    }

	return depth;
}

#include <stdio.h>
static char call_stack_info[_CALL_STACK_MAX_DEPTH * (8 + 1)] = { 0 };
static void _print_call_stack(const struct arch_esf *esf)
{
	size_t i, cur_depth = 0;
	uint32_t call_stack_buf[_CALL_STACK_MAX_DEPTH] = {0};
	cur_depth = _backtrace_call_stack(call_stack_buf, _CALL_STACK_MAX_DEPTH, esf);

    for (i = 0; i < cur_depth; i++) {
        sprintf(call_stack_info + i * (8 + 1), "%08lx", (unsigned long)call_stack_buf[i]);
        call_stack_info[i * (8 + 1) + 8] = ' ';
    }

	if (cur_depth) {
        call_stack_info[cur_depth * (8 + 1) - 1] = '\0';
        LOG_ERR("Show more call stack info by run: addr2line -e build/zephyr/zephyr.elf -a -f -p %s", call_stack_info);
    } else {
        LOG_ERR("Dump call stack has an error");
    }
}

#endif

#if CONFIG_CSK6_FATAL_UNHANDLE_INTERRUPT
const static uint8_t *interrupt_names[] = {
    "dma",
    "usbc",
    "sdio",
    "crypto",
    "qspi",
    "efuse",
    "timer",
    "wdt",
    "uart0",
    "uart1",
    "uart2",
    "ir",
    "spi0",
    "spi1",
    "iic0",
    "iic1",
    "gpt",
    "gpio0",
    "gpio1",
    "gpadc",
    "trng",
    "cmn-mailbox",
    "cmn-uart",
    "aon-keysense",
    "aon-cbutton",
    "aon-rtc",
    "aon-iwdt",
    "aon-timer",
    "aon-wakeup",
    "aon-pmuc",
    "aon-cp0",
    "aon-cp1",
    "aon-cp2",
};

void csk6_fatal_unhandle_interrupt_print(void)
{
    uint8_t irq_num = ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) >> SCB_ICSR_VECTACTIVE_Pos);
    const char* irq_name = "unknown";

    if (irq_num >= 16) {
        /* interrupt num */
        irq_num  = irq_num - 16;
        if (irq_num < sizeof(interrupt_names) / sizeof(uint8_t *)) {
            irq_name = interrupt_names[irq_num];
        }
    } else {
        /* exception num */
    }

    LOG_ERR("unhandle_interrupt, num:%d, name:%s", irq_num, irq_name);
}
#endif

void k_sys_fatal_error_handler(unsigned int reason,
				      const struct arch_esf *esf)
{
    LOG_ERR("*** CSK FATAL ***");
#if CONFIG_CSK6_FATAL_UNHANDLE_INTERRUPT
    if (reason == K_ERR_SPURIOUS_IRQ) {
        csk6_fatal_unhandle_interrupt_print();
    }
#endif

#if CONFIG_CSK6_FATAL_BACKTRACE
    _print_call_stack(esf);
#endif

	LOG_PANIC();
	LOG_ERR("Halting system");
	arch_system_halt(reason);
	CODE_UNREACHABLE; /* LCOV_EXCL_LINE */
}
