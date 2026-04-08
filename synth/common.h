/*
;************************************************************************
;  Name: Logan Burns
;  Description: Common compile-time constants and globals
;************************************************************************
*/

#ifndef COMMON_H_
#define COMMON_H_

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <stdint.h>
#include <avr/pgmspace.h>

//------------------------------------ENUMs----------------------------------------
typedef enum {
	ATTACK,
	DECAY,
	SUSTAIN,
	RELEASE,
	OFF
} EnvelopeState;

enum WaveType {
	SINE,
	TRIANGLE,
	SAWTOOTH,
	SQUARE,
	N_WAVES
};

//------------------------------------Structs----------------------------------------
typedef struct {
	EnvelopeState env_state;
	uint8_t note_id;
	uint32_t phase;			// phase accumulator
	uint32_t step;			// phase increment per sample
	uint16_t gain;			// current envelope gain
	int16_t lpf_state;		// 1-pole low-pass filter state
	uint8_t seen_this_block;
	uint8_t hold_ticks;
}Voice;

// CLOCK
#define FCLK 32000000u
#define pre 1u
#define NPTS 256u
#define FS 20000u
#define denominator ((uint32_t)pre * (uint32_t)FS)
#define average_period (((uint32_t)FCLK + (denominator/2))/(denominator))
#define FS_REAL ((int32_t)(((uint64_t)FCLK)/((uint64_t)pre * (uint64_t)average_period)))

// Voices
#define N_VOICES	3u // Number of voices that will be handled
#define MAX_VOICES	8u

#define PHASE_BITS	32u // Phase accumulator width
#define LUT_BITS	8u // Number of bits in our sampled wave LUT
#define PHASE_SHIFT (PHASE_BITS - LUT_BITS) // How far we will shift the phase to the right to get the current sample
#define HOLD_TICKS	6u // number of blocks to hold a note for
#define NOTE_LUT_LEN 88u

// DMA
#define BLOCK_SIZE	NPTS // Number of samples prefilled/half the size of the DMA ping-pong buffer.
#define KEYLEN		(N_VOICES * 3u)

// Effective time = (gain delta) / (rate * FS).
#define GAIN_FRAC		4u
#define PEAK_GAIN		2047u
#define PEAK_GAIN_FP	(PEAK_GAIN << GAIN_FRAC) // 32752
#define SUSTAIN_GAIN	1500u
#define SUSTAIN_GAIN_FP	(SUSTAIN_GAIN << GAIN_FRAC) // 24000
#define N_RATE_PRESETS	7u

// LFO vibrato
#define LFO_FREQ	5u
#define LFO_STEP	((uint32_t)(((uint64_t)LFO_FREQ * BLOCK_SIZE << PHASE_BITS) / FS_REAL))

// Low-pass filter
#define N_LPF_PRESETS	5u
#define LPF_SHIFT		8u

// Mixing
#define DAC_OFFSET 2048u
#define MIX_SHIFT  0u

#define DIV_APPROX_SHIFT		8u
#define RECIP_DIV(D)			((int32_t)(((1u << DIV_APPROX_SHIFT) + (D)/2u) / (D)))
#define DIV_APPROX_INT32(x, D)	((int32_t)(((int32_t)(x) * RECIP_DIV(D)) >> DIV_APPROX_SHIFT))
#define DIV_APPROX_UINT32(x, D)	((uint32_t)(((uint32_t)(x) * RECIP_DIV(D)) >> DIV_APPROX_SHIFT))


// ------- Drawing -------
// DAC to rows
#define SCOPE_WAVE_ROWS		16u
#define SCOPE_EXTRA_ROWS	2u
#define SCOPE_ROWS			(SCOPE_WAVE_ROWS + SCOPE_EXTRA_ROWS)

#define SCOPE_DAC_BITS		12u
#define SCOPE_ROW_BITS		4u
#define SCOPE_SHIFT			(SCOPE_DAC_BITS - SCOPE_ROW_BITS) // how much we will shift each scope term by to fit in putty window

#define SCOPE_ROW_FROM_DAC(v) ((uint8_t)((v) >> SCOPE_SHIFT)) // shift each term to fit in the 16 rows for PuTTy
#define WAVE_ROW_FROM_DAC(v) ((uint8_t)(SCOPE_WAVE_ROWS - 1u - SCOPE_ROW_FROM_DAC(v))) // inversion of above macro for wave fill-in

// Columns
#define SCOPE_COLS 128u
#define SCOPE_COLS_USED SCOPE_COLS
#define SCOPE_SAMPLES_PER_FRAME (BLOCK_SIZE * 2u)
#define SCOPE_DECIMATE_FACTOR 2u
#define SCOPE_VISIBLE_SAMPLES (SCOPE_COLS_USED * SCOPE_DECIMATE_FACTOR)

#define SCOPE_SAMPLE_INDEX_FROM_COL(col) ((uint16_t)(col) * SCOPE_DECIMATE_FACTOR)

// stepping through rows, 2 characters for EOL
#define SCOPE_STRIDE (SCOPE_COLS + 2u)
#define SCOPE_ESC_LEN 6u // ESC[J (clear below) + ESC[H (cursor home)
#define SCOPE_FRAME_CHAR_BYTES (SCOPE_ROWS * SCOPE_STRIDE)
#define SCOPE_FRAME_BYTES      (SCOPE_FRAME_CHAR_BYTES + SCOPE_ESC_LEN)

// general characters
#define SCOPE_CHAR_BG      ' '
#define SCOPE_CHAR_TRACE   '*'
#define SCOPE_CHAR_AXIS    '|'
#define SCOPE_CHAR_ZERO    '-'
#define SCOPE_CHAR_CORNER  '+'

#define SCOPE_INFO_ROW (SCOPE_WAVE_ROWS + 1u)
#define SCOPE_INFO_LABEL "Active Voices: "
#define SCOPE_INFO_LABEL_LEN (sizeof(SCOPE_INFO_LABEL) - 1u)
#define SCOPE_INFO_VOICE_POS (SCOPE_INFO_LABEL_LEN)
#define SCOPE_FRAME_SKIP 4u

#if N_VOICES > MAX_VOICES
#error "N_VOICES must be <= MAX_VOICES"
#endif
#if SCOPE_VISIBLE_SAMPLES > SCOPE_SAMPLES_PER_FRAME
#error "Scope: visible window exceeds available samples"
#endif

//------------------------------------Extern LUTs----------------------------------------
extern const int16_t  sine_lut[NPTS];
extern const int16_t  triangle_lut[NPTS];
extern const int16_t  sawtooth_lut[NPTS];
extern const int16_t  square_lut[NPTS];
extern const uint16_t note_lut[NOTE_LUT_LEN];
extern const int16_t* const waves[N_WAVES];

//------------------------------------Extern Globals----------------------------------------
// PuTTY frame buffer
extern volatile uint8_t scope_seq;
extern char scope_frame_bank[2][SCOPE_FRAME_BYTES];
extern volatile uint8_t scope_frame_tx;     // 0/1: DMA reads this
extern volatile uint8_t scope_frame_build;  // 0/1: scope_update_wave writes this
extern uint8_t scope_prev_row[SCOPE_COLS_USED]; // each index will store the wave row corresponding to that column. will make updating the wave WAYYYYY faster :3

extern Voice voices[N_VOICES];
extern uint16_t pfbuff[2][BLOCK_SIZE]; // Ping-pong buffer for DMA writes

extern volatile uint8_t cwave; // Default wave setting, will be alternated when the corresponding key is read via USART
extern volatile uint8_t cbuff; // 0 or 1, selects which prefill buffer to use
extern volatile uint8_t fillflag; // Set by DMA ISR when it is time to fill

extern uint8_t base; // Key offset, octaves, changed with zxcvbnm, (the comma accesses the bottom 3 notes although they sound like crap on this speaker)
extern uint8_t active_idx[N_VOICES];
extern uint8_t n_active;

extern uint32_t freq_lut[NOTE_LUT_LEN];
extern int8_t note_owner[NOTE_LUT_LEN];

extern uint8_t keybuff[KEYLEN]; // number of key events to buffer

extern const int16_t recip_table[MAX_VOICES + 1u];


// ADSR rate presets — indexed by [0=attack, 1=decay, 2=release]
extern const uint8_t  adsr_rates[3][N_RATE_PRESETS];
extern const uint16_t adsr_ms[3][N_RATE_PRESETS];
extern uint8_t adsr_idx[3];
extern uint8_t adsr_rate[3];

// LFO state
extern uint32_t lfo_phase;
extern volatile uint8_t vibrato_on;

// Low-pass filter
extern const uint16_t lpf_alpha[N_LPF_PRESETS];
extern uint8_t lpf_idx;

// visualizer flags
extern volatile uint8_t scope_busy;
extern volatile uint8_t scope_ready;
extern volatile uint8_t scope_block_counter;
extern volatile uint8_t lock_cbuff;

#endif /* COMMON_H_ */