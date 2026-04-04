/*
;************************************************************************
;  Name: Logan Burns
;  Description: DMA init and functions
;************************************************************************
*/

#ifndef DMA_H_
#define DMA_H_

void init_dma_dac(void);
void init_dma_usart(void);
void set_src_addr(uint8_t cbuff);
void set_scope_src_addr(uint8_t bank);

#endif /* DMA_H_ */