/*
;************************************************************************
;  Name: Logan Burns
;  Description: scope functions
;************************************************************************
*/

#include <avr/io.h>
#include "common.h"


#ifndef SCOPE_H_
#define SCOPE_H_

void scope_set_active_voices(void);
uint16_t scope_get_sample(uint16_t sample_idx);
void scope_update_wave(void);
void init_frame(void);

#endif /* SCOPE_H_ */