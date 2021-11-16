#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "soc/gpio_struct.h"
#include "soc/timer_group_struct.h"
#include "soc/rtc_cntl_struct.h"
#include "soc/uart_struct.h"

#define LED 2

volatile int v;

void __attribute__((noreturn)) __pro_cpu_main(void) {
	bool state = true;
	TIMERG0.wdtconfig0.val = 0;
	TIMERG1.wdtconfig0.val = 0;
	RTCCNTL.wdt_config0.val = 0;

	gpio_dev_t* gpio = &GPIO;

	gpio->func_out_sel_cfg[LED].func_sel = 0x100;
	gpio->enable_w1ts = 1<<LED;
	
	for (;;)
	{
		//kprintf("flash %d!\n", state);
		kputchar('0'+state);
		if (state)
			gpio->out_w1ts = 1<<LED;
		else
			gpio->out_w1tc = 1<<LED;

		state = !state;

		v = 1000000;
		while (v--)
			;
	}
}

uint16_t swap_dev = 0xffff;
uint8_t need_resched;

void map_init(void)
{
	panic("map_init");
}

void platform_discard(void) {}

void platform_monitor(void)
{
	while(1)
		asm volatile ("waiti 15");
}

void platform_reboot(void)
{
	panic("reboot");
}

uint_fast8_t platform_param(char* p)
{
	return 0;
}

