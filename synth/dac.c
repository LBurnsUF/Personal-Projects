/*
;************************************************************************
;  Name: Logan Burns
;  Description: DAC init and functions
;************************************************************************
*/

#include <avr/io.h>
#include "common.h"

void init_dac(void){
	DACA.CTRLB = DAC_CHSEL_SINGLE1_gc | DAC_CH1TRIG_bm;
	DACA.CTRLC = DAC_REFSEL_AREFB_gc;

	PORTCFG.MPCMASK = PIN2_bm | PIN3_bm;
	PORTA.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
	DACA.EVCTRL = DAC_EVSEL_0_gc;

	DACA.CTRLA = DAC_CH1EN_bm | DAC_CH0EN_bm | DAC_ENABLE_bm;
}

