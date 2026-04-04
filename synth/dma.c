/*
;************************************************************************
;  Name: Logan Burns
;  Description: handles all DMA initialization
;************************************************************************
*/

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include "common.h"
#include "dma.h"

//----------------------------DMA CH0 init (DAC)--------------------------------
// CH0 always handles outputting samples to dac from filled ping-pong buffer
void init_dma_dac(void){
	// Source increments. Increment from DATAL -> DATAH.
	DMA.CH0.ADDRCTRL = DMA_CH_SRCRELOAD_BLOCK_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_BURST_gc | DMA_CH_DESTDIR_INC_gc;

	uintptr_t dst = (uintptr_t)&DACA.CH1DATAL;
	uintptr_t src = (uintptr_t)&pfbuff[cbuff][0];

	DMA.CH0.SRCADDR0 = (uint8_t)((uint32_t)src & 0xFF);
	DMA.CH0.SRCADDR1 = (uint8_t)(((uint32_t)src >> 8) & 0xFF);
	DMA.CH0.SRCADDR2 = (uint8_t)(((uint32_t)src >> 16) & 0xFF);

	// Always send points to DAC
	DMA.CH0.DESTADDR0 = (uint8_t)((uint32_t)dst & 0xFF);
	DMA.CH0.DESTADDR1 = (uint8_t)(((uint32_t)dst >> 8) & 0xFF);
	DMA.CH0.DESTADDR2 = (uint8_t)(((uint32_t)dst >> 16) & 0xFF);

	// Initialize single shot on each TCC0 overflow
	DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_EVSYS_CH0_gc;

	DMA.CH0.REPCNT = 0;					// Unlimited Repeat
	DMA.CH0.TRFCNT = BLOCK_SIZE * 2;	// 16 bit samples, 2 bytes per DMA event

	DMA.CH0.CTRLB = DMA_CH_TRNINTLVL_MED_gc;
	PMIC.CTRL |= PMIC_MEDLVLEN_bm;

	// Burst 2 bytes per event, single-shot per trigger
	DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm | DMA_CH_BURSTLEN_2BYTE_gc | DMA_CH_SINGLE_bm;
	DMA.CTRL = DMA_ENABLE_bm;
}

void init_dma_usart(void){
	// Fixed source, increment destination within keybuff[]
	DMA.CH1.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTRELOAD_BLOCK_gc | DMA_CH_DESTDIR_INC_gc;

	uintptr_t src = (uintptr_t)&USARTD0.DATA;
	uintptr_t dst = (uintptr_t)&keybuff[0];

	DMA.CH1.SRCADDR0 = (uint8_t)((uint32_t)src & 0xFF);
	DMA.CH1.SRCADDR1 = (uint8_t)(((uint32_t)src >> 8) & 0xFF);
	DMA.CH1.SRCADDR2 = (uint8_t)(((uint32_t)src >> 16) & 0xFF);

	// Send points to keybuff
	DMA.CH1.DESTADDR0 = (uint8_t)((uint32_t)dst & 0xFF);
	DMA.CH1.DESTADDR1 = (uint8_t)(((uint32_t)dst >> 8) & 0xFF);
	DMA.CH1.DESTADDR2 = (uint8_t)(((uint32_t)dst >> 16) & 0xFF);

	// Trigger on each received byte
	DMA.CH1.TRIGSRC = DMA_CH_TRIGSRC_USARTD0_RXC_gc;

	DMA.CH1.REPCNT = 0;			// Unlimited Repeat
	DMA.CH1.TRFCNT = KEYLEN;	// Circular buffer size in bytes. N bytes for type, N bytes for keys

	// Burst 1 byte per character received
	DMA.CH1.CTRLA |= DMA_CH_ENABLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm | DMA_CH_SINGLE_bm;

	DMA.CH2.ADDRCTRL = DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_FIXED_gc;

	uintptr_t dst2 = (uintptr_t)&USARTD0.DATA;
	uintptr_t src2 = (uintptr_t)&scope_frame_bank[scope_frame_tx][0];

	DMA.CH2.SRCADDR0 = (uint8_t)((uint32_t)src2 & 0xFF);
	DMA.CH2.SRCADDR1 = (uint8_t)(((uint32_t)src2 >> 8) & 0xFF);
	DMA.CH2.SRCADDR2 = (uint8_t)(((uint32_t)src2 >> 16) & 0xFF);

	DMA.CH2.DESTADDR0 = (uint8_t)((uint32_t)dst2 & 0xFF);
	DMA.CH2.DESTADDR1 = (uint8_t)(((uint32_t)dst2 >> 8) & 0xFF);
	DMA.CH2.DESTADDR2 = (uint8_t)(((uint32_t)dst2 >> 16) & 0xFF);

	DMA.CH2.TRFCNT = SCOPE_FRAME_BYTES;	// Frame buffer size in bytes

	// Burst 1 byte per DRE
	DMA.CH2.TRIGSRC = DMA_CH_TRIGSRC_USARTD0_DRE_gc;
	DMA.CH2.CTRLA = DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_SINGLE_bm;

	DMA.CTRL |= DMA_ENABLE_bm;
}

void set_src_addr(uint8_t cbuff){
	uintptr_t src = (uintptr_t)&pfbuff[cbuff][0];

	DMA.CH0.SRCADDR0 = (uint8_t)((uint32_t)src & 0xFF);
	DMA.CH0.SRCADDR1 = (uint8_t)(((uint32_t)src >> 8) & 0xFF);
	DMA.CH0.SRCADDR2 = (uint8_t)(((uint32_t)src >> 16) & 0xFF);
}

void set_scope_src_addr(uint8_t bank)
{
	uintptr_t src = (uintptr_t)&scope_frame_bank[bank][0];

	DMA.CH2.SRCADDR0 = (uint8_t)((uint32_t)src & 0xFF);
	DMA.CH2.SRCADDR1 = (uint8_t)(((uint32_t)src >> 8) & 0xFF);
	DMA.CH2.SRCADDR2 = (uint8_t)(((uint32_t)src >> 16) & 0xFF);
}
