/*
;************************************************************************
;  Name: Logan Burns
;  Description: Timer init and functions
;************************************************************************
*/

#include <stdint.h>

#ifndef USART_H_
#define USART_H_

void tcc0_init(uint16_t period, uint8_t wgmode, uint8_t tc_clksel, uint8_t is_event_source);

#endif /* USART_H_ */