/*
;************************************************************************
;  Name: Logan Burns
;  Description: Modify the system clock frequency
;************************************************************************
*/

#include <avr/io.h>
#include <avr/cpufunc.h>
#include "clock.h"
#include "common.h"

void init_clk(void){
	OSC.CTRL |= OSC_RC32MEN_bm; // Enable the reference clock source
	while (!(OSC.STATUS & OSC_RC32MRDY_bm));
	OSC.CTRL |= OSC_RC32KEN_bm;
	while(!(OSC.STATUS & OSC_RC32KRDY_bm));
	OSC.DFLLCTRL = (OSC.DFLLCTRL & ~OSC_RC32MCREF_gm) | OSC_RC32MCREF_RC32K_gc;
	DFLLRC32M.CTRL = DFLL_ENABLE_bm;
	ccp_write_io((uint8_t*)&CLK.CTRL,CLK_SCLKSEL_RC32M_gc); // Switch system clock to 32 MHz
	ccp_write_io((uint8_t*)&CLK.PSCTRL,(CLK_PSADIV_1_gc | CLK_PSBCDIV_1_1_gc)); // ensure no prescaling
}