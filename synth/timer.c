/*
;************************************************************************
;  Name: Logan Burns
;  Description: Timer init and functions
;************************************************************************
*/

#include <avr/io.h>
#include <stdint.h>
#include "timer.h"

void tcc0_init(uint16_t period, uint8_t wgmode, uint8_t tc_clksel, uint8_t is_event_source){
	TCC0.CTRLFSET = TC_CMD_RESET_gc; // reset timer before configuration
	TCC0.PER = period - 1;
	TCC0.CTRLB = wgmode;
	TCC0.CTRLA = tc_clksel;
	if(is_event_source){
		EVSYS.CH0MUX = EVSYS_CHMUX_TCC0_OVF_gc;
	}
}

inline void tcc0_set_period(uint16_t period){
	TCC0.PER = (uint16_t)(period - 1);
}
inline void tcc0_start(uint8_t tc_clksel){
	TCC0.CTRLA = tc_clksel;
	}
inline void tcc0_stop(void){
	TCC0.CTRLA = TC_CLKSEL_OFF_gc;
	}