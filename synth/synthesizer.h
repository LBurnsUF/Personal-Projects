/*
;************************************************************************
;  Simple Polyphonic Synthesizer
;  Name: Logan Burns
;************************************************************************
*/

#ifndef SYNTHESIZER_H_
#define SYNTHESIZER_H_

void init_dac(void);
void init_dma_dac(void);
void init_dma_usart(void);
void init_usart(void);

void blockfill(uint16_t block[]);
void update_voices_from_keybuff(void);

#endif /* SYNTHESIZER_H_ */