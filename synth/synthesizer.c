/*
;************************************************************************
;  Name: Logan Burns
;  Description: Basic polyphonic synthesizer
;************************************************************************
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <avr/pgmspace.h>
#include "common.h"
#include "timer.h"
#include "synthesizer.h"
#include "clock.h"
#include "dma.h"
#include "dac.h"
#include "scope.h"

//------------------------------------local prototypes----------------------------------------
int8_t ascii_to_note_id(uint8_t ch);
inline void init_freq_lut(void);
inline int32_t fmul16x16_shift10(int16_t scaled, uint16_t gain);

int main(void)
{
	init_clk();
	init_dac();
	init_freq_lut();
	init_frame();

	// initialize voices to OFF
	for (uint8_t v = 0; v < N_VOICES; ++v) {
		voices[v].env_state = OFF;
		voices[v].note_id = 0;
		voices[v].phase = 0;
		voices[v].step = 0;
		voices[v].gain = 0;
		voices[v].seen_this_block = 0;
		voices[v].hold_ticks = 0;
	}

	// prefill both pfbuff banks with silence
	for (uint8_t b = 0; b < 2; ++b) {
		for (uint16_t i = 0; i < BLOCK_SIZE; ++i) {
			pfbuff[b][i] = DAC_OFFSET;
		}
	}

	// Clear key buffer
	for (uint8_t i = 0; i < KEYLEN; ++i) {
		keybuff[i] = 0;
	}

	// initialize ownership table
	for (uint8_t i = 0; i < NOTE_LUT_LEN; ++i) {
		note_owner[i] = -1;
	}

	tcc0_init(average_period, TC_WGMODE_NORMAL_gc, TC_CLKSEL_DIV1_gc, 1);
	TCC0.INTFLAGS = TC0_CCAIF_bm | TC0_OVFIF_bm; // Clear flags
	init_dma_usart();
	init_usart();
	sei();
	init_dma_dac();

	// overhead measurements
	PORTC.DIRSET = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm;

	// Pull Power-Down high
	PORTC.DIRSET = PIN7_bm;
	PORTC.OUTSET = PIN7_bm;

	while (1)
	{
		if((DMA.INTFLAGS & DMA_CH2TRNIF_bm)){
			scope_busy = 0;
			DMA.INTFLAGS = DMA_CH2TRNIF_bm;
		}

		if(fillflag) {
			fillflag = 0;
			PORTC.OUTSET = PIN0_bm | PIN3_bm;
			update_voices_from_keybuff();
			PORTC.OUTCLR = PIN3_bm;

			lock_cbuff = cbuff;
			PORTC.OUTSET = PIN2_bm;
			blockfill(pfbuff[1 ^ lock_cbuff]); // fill next half
			PORTC.OUTCLR = PIN2_bm;
			if (++scope_block_counter >= SCOPE_FRAME_SKIP) {
				scope_block_counter = 0;
				PORTC.OUTSET = PIN4_bm;
				scope_update_wave();   // always build/publish newest frame
				PORTC.OUTCLR = PIN4_bm;
			}
			PORTC.OUTCLR = PIN0_bm;
		}

		if (scope_ready && !scope_busy) {
			scope_busy = 1;
			scope_ready = 0;

			DMA.INTFLAGS = DMA_CH2TRNIF_bm;
			DMA.CH2.TRFCNT = SCOPE_FRAME_BYTES;
			set_scope_src_addr(scope_frame_tx);
			DMA.CH2.CTRLA = DMA_CH_ENABLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_SINGLE_bm;
		}
	}
}

//------------------------------------ISRs----------------------------------------
// on transaction completion, signal main to fill other half
// and swap immediately to precomputed half to avoid gaps in output
ISR(DMA_CH0_vect){
 PORTC.OUTTGL = PIN1_bm;
 cbuff ^= 1; // toggle cbuff
 DMA.INTFLAGS = DMA_CH0TRNIF_bm; // clear transfer flag
 set_src_addr(cbuff); // update source address
 DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm; // Start transfer of other half
 fillflag = 1;
}

//------------------------------------helpers------------------------------------------
int8_t ascii_to_note_id(uint8_t ch){
	if (ch == 's') {
		cwave ^= 1; // toggle wave type
		return -1;
	}
	if (ch == 'a') {
		vibrato_on ^= 1; // toggle vibrato
		return -1;
	}
	if (ch >= '1' && ch <= '3') {
		uint8_t p = ch - '1'; // 0=attack, 1=decay, 2=release
		adsr_idx[p] = (adsr_idx[p] + 1) % N_RATE_PRESETS;
		adsr_rate[p] = pgm_read_byte(&adsr_rates[p][adsr_idx[p]]);
		return -1;
	}
	 uint8_t idx;
	// interpret other characters
	switch(ch){
		case 'e': idx = base + 0; break;
		case '4': idx = base + 1; break;
		case 'r': idx = base + 2; break;
		case '5': idx = base + 3; break;
		case 't': idx = base + 4; break;
		case 'y': idx = base + 5; break;
		case '7': idx = base + 6; break;
		case 'u': idx = base + 7; break;
		case '8': idx = base + 8; break;
		case 'i': idx = base + 9; break;
		case '9': idx = base + 10; break;
		case 'o': idx = base + 11; break;
		case 'p': idx = base + 12; break;
		// Key changes (octaves)
		case ',': base = 0; return -1; // give access to lowest 3 notes
		case 'z': base = 3; return -1; // C1
		case 'x': base = 15; return -1; // C2
		case 'c': base = 27; return -1; // C3
		case 'v': base = 39; return -1; // C4
		case 'b': base = 51; return -1; // C5
		case 'n': base = 63; return -1; // C6
		case 'm': base = 75; return -1; // C7
		default: return -1;
	}
		return (int8_t)idx;
}

//------------------------------------Synth Functions------------------------------------------
void update_voices_from_keybuff(void){
	for (uint8_t v = 0; v < N_VOICES; ++v) {
		voices[v].seen_this_block = 0; 	// clear seen flags
	}
	for (uint16_t i = 0; i < KEYLEN; ++i) {
		if (keybuff[i] == 0) {
        continue; // no byte received here this block
		}

		int8_t note_id = ascii_to_note_id(keybuff[i]);
		keybuff[i] = 0; // prevent cycling the same buffered keys if the user stops holding keys

		if (note_id < 0) {
			 continue; // Skip non-notes
		}

		int8_t owner = note_owner[note_id];

		if (owner >= 0) {
			Voice *v = &voices[owner];

			if (v->env_state != OFF) {
				v->seen_this_block = 1;
				v->hold_ticks = HOLD_TICKS;
				if (v->env_state == RELEASE) {
					v->env_state = ATTACK; // retrigger from current gain
				}
				continue;
			}
			else {
				note_owner[note_id] = -1; // stale, clear ownership
			}
		}

		// Steal constants
		int8_t free_voice    = -1;
		int8_t best_release  = -1;
		int8_t chosen = -1;
		uint16_t best_rel_gain = UINT16_MAX;

		// The received note is not already active, scan all voices once for best steal candidate.
		// Prefer an OFF voice, otherwise, steal the RELEASE voice closest to silent (lowest gain)
		for (uint8_t v = 0; v < N_VOICES; ++v) {
			if (voices[v].env_state == OFF) {
				free_voice = (int8_t)v;
				break;
			}
			if (voices[v].env_state == RELEASE) {
				uint16_t g = voices[v].gain;
				if (g <= best_rel_gain) {
					best_rel_gain = g;
					best_release = (int8_t)v;
				}
			}
		}

		if (free_voice >= 0) {
			chosen = free_voice; // use free OFF voice
		}
		else if (best_release >= 0) {
			chosen = best_release; // steal oldest RELEASED voice
		}
		else {
			continue;
		}

		Voice *voice = &voices[chosen];

		if (voice->env_state != OFF) {
			uint8_t old_id = voice->note_id;
			if (note_owner[old_id] == chosen) {
				note_owner[old_id] = -1;
			}
		}

		voice->env_state = ATTACK;
		voice->gain = 0;
		voice->note_id = (uint8_t)note_id;
		voice->phase = 0;
		voice->step = freq_lut[note_id];
		voice->seen_this_block = 1;
		voice->hold_ticks = HOLD_TICKS;
		note_owner[note_id] = chosen; // update note_owner table
	}

	// Transition released keys to RELEASE
	for (uint8_t v = 0; v < N_VOICES; ++v) {
		if(voices[v].env_state == OFF || voices[v].env_state == RELEASE) continue;
		if(!voices[v].seen_this_block && voices[v].hold_ticks > 0){
			voices[v].hold_ticks -= 1; // decrement hold
		}
		else if (voices[v].hold_ticks == 0)
		{
			voices[v].env_state	= RELEASE;
		}
	}
}

void blockfill(uint16_t block[]){

	// list all active voices
	n_active = 0;
	for (uint8_t v = 0; v < N_VOICES; ++v) {
		if (voices[v].env_state != OFF) {
			active_idx[n_active++] = v;
		}
	}

	if (n_active == 0) {
        for (uint16_t i = 0; i < BLOCK_SIZE; ++i) {
            block[i] = DAC_OFFSET;
        }
        return;
    }

	const int16_t *wave = waves[cwave];
	int16_t recip = recip_table[n_active];

	// LFO: compute modulated steps once per block (or bypass if vibrato off)
	uint32_t mod_step[N_VOICES];
	if (vibrato_on) {
		lfo_phase += LFO_STEP;
		int16_t lfo_val = (int16_t)pgm_read_word(&sine_lut[(uint8_t)(lfo_phase >> 24)]);
		for (uint8_t k = 0; k < n_active; ++k) {
			uint32_t step = voices[active_idx[k]].step;
			// offset ≈ ±step/128 ≈ ±13 cents
			int32_t offset = ((int32_t)(step >> 10) * lfo_val) >> 7;
			mod_step[k] = (uint32_t)((int32_t)step + offset);
		}
	} else {
		for (uint8_t k = 0; k < n_active; ++k) {
			mod_step[k] = voices[active_idx[k]].step;
		}
	}

	for (uint16_t i = 0; i < BLOCK_SIZE; ++i) {
		int32_t mix = 0;
		for (uint8_t k = 0; k < n_active; ++k) {
			Voice *voice = &voices[active_idx[k]];
			uint16_t gain;

			switch (voice->env_state) {
			case ATTACK:
				voice->gain += adsr_rate[0];
				if (voice->gain >= PEAK_GAIN_FP) {
					voice->gain = PEAK_GAIN_FP;
					voice->env_state = DECAY;
				}
				gain = voice->gain >> GAIN_FRAC;
				break;
			case DECAY:
				if (voice->gain > SUSTAIN_GAIN_FP + adsr_rate[1]) {
					voice->gain -= adsr_rate[1];
				} else {
					voice->gain = SUSTAIN_GAIN_FP;
					voice->env_state = SUSTAIN;
				}
				gain = voice->gain >> GAIN_FRAC;
				break;
			case SUSTAIN:
				gain = SUSTAIN_GAIN;
				break;
			case RELEASE:
				if (voice->gain > adsr_rate[2]) {
					voice->gain -= adsr_rate[2];
				} else {
					voice->gain = 0;
					note_owner[voice->note_id] = -1;
					voice->env_state = OFF;
				}
				gain = voice->gain >> GAIN_FRAC;
				break;
			case OFF:
			default:
				continue;
			}

			// Oscillator step (LFO-modulated)
			voice->phase += mod_step[k];
			uint8_t index = (uint8_t)(voice->phase >> PHASE_SHIFT);
			int16_t s = (int16_t)pgm_read_word(&wave[index]);

			int32_t scaled = fmul16x16_shift10(s, gain);
			mix += (int32_t)scaled;
		}


        mix = (int32_t)((mix * recip) >> DIV_APPROX_SHIFT); // approximate average

		// clamp to 12-bit DAC range
		int32_t out = mix + DAC_OFFSET; // recenter output about center of voltage range
		if (out < 0) {
			out = 0;
		}
		if (out > 4095) {
			out = 4095;
		}
		block[i] = (uint16_t)out;
	}
}

//----------------------------Synthesizer initialization----------------------------------
inline void init_freq_lut(void){
	for(int i = 0; i < NOTE_LUT_LEN; i++){
		uint16_t f = (uint16_t)pgm_read_word(&note_lut[i]);
		freq_lut[i] = (uint32_t)(((uint64_t)f << PHASE_BITS) / FS_REAL); // step = (f * 2^PHASE_BITS) / Fs.
	}
	return;
}

//----------------------------inine ASM primitives----------------------------------
inline int32_t fmul16x16_shift10(int16_t scaled, uint16_t gain){
	uint32_t result;
	uint8_t neg;

	if (scaled < 0) {
		scaled = (uint16_t)(-scaled);
		neg = 1;
		} else {
		scaled = (uint16_t)scaled;
		neg = 0;
	}

	// A general explanation if I ever share this code is:
	// a 32 bit multiply can be broken down as
	// x = x_high<<8 + x_low, y = y_high<<8 + y_low
	// x * y = (x_high<<8 + x_low)(y_high<<8 + y_low) = x_low * y_low +  (x_low * y_high) << 8 + (x_high * y_low) << 8 + (x_high * y_high) << 16

	// with inline ASM and one output, %1 is scaled, %2 is gain.
	// %A1 is scaled low byte, %B1 scaled high byte
	// %A2 is gain low byte, %B2 gain high byte
	// %A0 %B0 %C0 %D0 are the result bytes, in ascending order by significance

	asm volatile (
	// x_low * y_low
	"mul %A1, %A2 \n\t"
	"mov %A0, r0 \n\t" // result_0
	"mov %B0, r1 \n\t" // result_1
	"clr %C0 \n\t" // result_2
	"clr %D0 \n\t" // result_3

	// (x_low * y_high) << 8
	"mul %A1, %B2 \n\t"
	"add %B0, r0 \n\t" // result_1 += low
	"adc %C0, r1 \n\t" // result_2 += high + carry

	// (x_high * y_low) << 8
	"mul %B1, %A2 \n\t"
	"add %B0, r0 \n\t" // result_1 += low
	"adc %C0, r1 \n\t" // result_2 += high + carry
	"clr r1 \n\t"
	"adc %D0, r1 \n\t" // result_3 += carry

	// (x_high * y_high) << 16
	"mul %B1, %B2 \n\t"
	"add %C0, r0 \n\t" // result_2 += low
	"adc %D0, r1 \n\t" // result_3 += high + carry

	// drop low byte for gain shift ( >>8)
	"mov %A0, %B0 \n\t"
	"mov %B0, %C0 \n\t"
	"mov %C0, %D0 \n\t"
	"clr %D0 \n\t"

	// >>2 to get >>10 total
	"ror %C0 \n\t"
	"ror %B0 \n\t"
	"ror %A0 \n\t"
	"clc \n\t"
	"ror %C0 \n\t"
	"ror %B0 \n\t"
	"ror %A0 \n\t"

	// restore zero register
	"clr r1 \n\t"
	: "=&r"(result) // outputs
	: "r"((uint16_t)scaled), "r"(gain) // inputs
	: "r0", "r1", "cc" // clobbers
	); // 32 cycles total by my count

	return neg ? -(int32_t)result : (int32_t)result;
}