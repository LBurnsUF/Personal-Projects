/*
;************************************************************************
;  Name: Logan Burns
;  Description: DMA init and functions
;************************************************************************
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "common.h"
#include "scope.h"

#define SCOPE_FRAME_BUILD_PTR (scope_frame_bank[scope_frame_build])
#define SCOPE_FRAME_TX_PTR    (scope_frame_bank[scope_frame_tx])
#define SCOPE_SEQ_LABEL_POS   (SCOPE_INFO_VOICE_POS + 2u)
#define SCOPE_SEQ_DIGIT_POS   (SCOPE_SEQ_LABEL_POS + 2u)

// Write uint16_t right-aligned into a fixed-width field, space-padded
static void scope_write_uint(char *dest, uint16_t val, uint8_t width)
{
	for (int8_t i = width - 1; i >= 0; --i) {
		if (val > 0 || i == (int8_t)(width - 1)) {
			dest[i] = '0' + (val % 10);
			val /= 10;
		} else {
			dest[i] = ' ';
		}
	}
}

static inline char *scope_build_text(void)
{
	return &SCOPE_FRAME_BUILD_PTR[0];
}

static inline void scope_write_home_suffix(char *frame)
{
	frame[SCOPE_FRAME_CHAR_BYTES + 0] = 0x1B;
	frame[SCOPE_FRAME_CHAR_BYTES + 1] = '[';
	frame[SCOPE_FRAME_CHAR_BYTES + 2] = 'J';
	frame[SCOPE_FRAME_CHAR_BYTES + 3] = 0x1B;
	frame[SCOPE_FRAME_CHAR_BYTES + 4] = '[';
	frame[SCOPE_FRAME_CHAR_BYTES + 5] = 'H';
}

static inline void scope_publish_frame(void)
{
	uint8_t sreg = SREG;
	cli();
	scope_frame_tx = scope_frame_build;
	scope_frame_build ^= 1u;
	SREG = sreg;

	scope_ready = 1;
}

static inline void scope_clear_wave_rows(char *text)
{
	for (uint8_t row = 0; row < SCOPE_WAVE_ROWS; ++row) {
		char *line = &text[row * SCOPE_STRIDE];
		for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
			line[col] = SCOPE_CHAR_BG;
		}
	}
}

static inline void scope_restore_static_rows(char *text)
{
	if (SCOPE_WAVE_ROWS < SCOPE_ROWS) {
		char *sep = &text[SCOPE_WAVE_ROWS * SCOPE_STRIDE];
		for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
			sep[col] = SCOPE_CHAR_ZERO;
		}
	}

	// info row
	if (SCOPE_INFO_ROW < SCOPE_ROWS) {
		char *info = &text[SCOPE_INFO_ROW * SCOPE_STRIDE];

		for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
			info[col] = SCOPE_CHAR_BG;
		}

		for (uint8_t pos = 0; pos < SCOPE_INFO_LABEL_LEN; ++pos) {
			info[pos] = SCOPE_INFO_LABEL[pos];
		}

		info[SCOPE_INFO_VOICE_POS] = (char)('0' + n_active);

		info[SCOPE_SEQ_LABEL_POS + 0] = 'S';
		info[SCOPE_SEQ_LABEL_POS + 1] = ':';
		info[SCOPE_SEQ_DIGIT_POS] = (char)('0' + (scope_seq % 10u));

		// Wave type
		uint8_t wp = SCOPE_SEQ_DIGIT_POS + 2u;
		info[wp + 0] = 'W';
		info[wp + 1] = ':';
		if (cwave == SINE) {
			info[wp + 2] = 'S'; info[wp + 3] = 'I'; info[wp + 4] = 'N';
		} else {
			info[wp + 2] = 'T'; info[wp + 3] = 'R'; info[wp + 4] = 'I';
		}

		// Vibrato
		uint8_t vp = wp + 6u;
		info[vp + 0] = 'V';
		info[vp + 1] = ':';
		info[vp + 2] = vibrato_on ? '1' : '0';

		// ADSR ms values, right-aligned in 4-char fields
		uint8_t ap = vp + 4u;
		info[ap + 0] = 'A'; info[ap + 1] = ':';
		scope_write_uint(&info[ap + 2], pgm_read_word(&adsr_ms[0][adsr_idx[0]]), 4);
		info[ap + 7] = 'D'; info[ap + 8] = ':';
		scope_write_uint(&info[ap + 9], pgm_read_word(&adsr_ms[1][adsr_idx[1]]), 4);
		info[ap + 14] = 'R'; info[ap + 15] = ':';
		scope_write_uint(&info[ap + 16], pgm_read_word(&adsr_ms[2][adsr_idx[2]]), 4);
	}
}

uint16_t scope_get_sample(uint16_t sample_idx)
{
	if (sample_idx < BLOCK_SIZE) {
		return pfbuff[lock_cbuff][sample_idx];
	}
	else if (sample_idx < SCOPE_SAMPLES_PER_FRAME) {
		return pfbuff[lock_cbuff ^ 1u][sample_idx - BLOCK_SIZE];
	}
	return DAC_OFFSET;
}

static void init_one_frame(char *frame)
{
	for (uint8_t row = 0; row < SCOPE_ROWS; ++row) {
		char *line = &frame[row * SCOPE_STRIDE];

		for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
			line[col] = SCOPE_CHAR_BG;
		}

		line[SCOPE_COLS + 0] = '\r';
		line[SCOPE_COLS + 1] = '\n';
	}

	if (SCOPE_WAVE_ROWS < SCOPE_ROWS) {
		for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
			frame[SCOPE_WAVE_ROWS * SCOPE_STRIDE + col] = SCOPE_CHAR_ZERO;
		}
	}

	if (SCOPE_INFO_ROW < SCOPE_ROWS) {
		for (uint8_t pos = 0; pos < SCOPE_INFO_LABEL_LEN; ++pos) {
			frame[SCOPE_INFO_ROW * SCOPE_STRIDE + pos] = SCOPE_INFO_LABEL[pos];
		}

		char *info = &frame[SCOPE_INFO_ROW * SCOPE_STRIDE];
		info[SCOPE_INFO_VOICE_POS] = '0';
		info[SCOPE_SEQ_LABEL_POS + 0] = 'S';
		info[SCOPE_SEQ_LABEL_POS + 1] = ':';
		info[SCOPE_SEQ_DIGIT_POS] = '0';

		uint8_t wp = SCOPE_SEQ_DIGIT_POS + 2u;
		info[wp + 0] = 'W'; info[wp + 1] = ':';
		info[wp + 2] = 'S'; info[wp + 3] = 'I'; info[wp + 4] = 'N';

		uint8_t vp = wp + 6u;
		info[vp + 0] = 'V'; info[vp + 1] = ':';
		info[vp + 2] = '0';

		uint8_t ap = vp + 4u;
		info[ap + 0] = 'A'; info[ap + 1] = ':';
		scope_write_uint(&info[ap + 2], pgm_read_word(&adsr_ms[0][adsr_idx[0]]), 4);
		info[ap + 7] = 'D'; info[ap + 8] = ':';
		scope_write_uint(&info[ap + 9], pgm_read_word(&adsr_ms[1][adsr_idx[1]]), 4);
		info[ap + 14] = 'R'; info[ap + 15] = ':';
		scope_write_uint(&info[ap + 16], pgm_read_word(&adsr_ms[2][adsr_idx[2]]), 4);
	}
	scope_write_home_suffix(frame);
}

void init_frame(void)
{
	for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
		scope_prev_row[col] = WAVE_ROW_FROM_DAC(DAC_OFFSET);
	}

	init_one_frame(scope_frame_bank[0]);
	init_one_frame(scope_frame_bank[1]);
}

void scope_update_wave(void)
{
	char *text = scope_build_text();
	scope_seq++;

	// find first rising zero-crossing
	uint16_t trigger_offset = 0;
	uint16_t search_limit = SCOPE_SAMPLES_PER_FRAME - SCOPE_VISIBLE_SAMPLES;
	for (uint16_t i = 0; i < search_limit; ++i) {
		uint16_t s0 = scope_get_sample(i);
		uint16_t s1 = scope_get_sample(i + 1);
		if (s0 < DAC_OFFSET && s1 >= DAC_OFFSET) {
			trigger_offset = i + 1;
			break;
		}
	}

	scope_clear_wave_rows(text);
	scope_restore_static_rows(text);
	scope_write_home_suffix(SCOPE_FRAME_BUILD_PTR);

	for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
		uint16_t sample_idx = trigger_offset + (uint16_t)col * SCOPE_DECIMATE_FACTOR;
		uint16_t dac = scope_get_sample(sample_idx);
		uint8_t new_row = WAVE_ROW_FROM_DAC(dac);

		text[new_row * SCOPE_STRIDE + col] = SCOPE_CHAR_TRACE;
		scope_prev_row[col] = new_row;
	}

	scope_publish_frame();
}