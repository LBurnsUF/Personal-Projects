/*
;************************************************************************
;  Name: Logan Burns
;  Description: USART init and functions
;************************************************************************
*/

#include <avr/io.h>
#include "common.h"
#include "usart.h"

void init_usart(void){

	PORTD.OUTSET = PIN3_bm;
	PORTD.DIRSET = PIN3_bm;	// TXD
	PORTD.DIRCLR = PIN2_bm;	// RXD

	// Maximize baud rate to minimize latency and allow clean output expansion
	USARTD0.BAUDCTRLA = (uint8_t)(1 & 0xFF);
	USARTD0.BAUDCTRLB = (uint8_t)(((int8_t)(0 & 0x0F) << USART_BSCALE_gp) | ((1 >> 8) & 0x0F));
	USARTD0.CTRLC = USART_CHSIZE_8BIT_gc | USART_PMODE_DISABLED_gc | USART_CMODE_ASYNCHRONOUS_gc;

	// Clear received buffer
	(void)USARTD0.DATA;
	USARTD0.STATUS = USART_TXCIF_bm;

	USARTD0.CTRLB = USART_RXEN_bm | USART_TXEN_bm |USART_CLK2X_bm;
}