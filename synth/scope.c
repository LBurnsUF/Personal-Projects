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

static inline char *scope_build_text(void)
{
	return &SCOPE_FRAME_BUILD_PTR[0];
}

static inline void scope_write_home_suffix(char *frame)
{
	frame[SCOPE_FRAME_CHAR_BYTES + 0] = 0x1B;
	frame[SCOPE_FRAME_CHAR_BYTES + 1] = '[';
	frame[SCOPE_FRAME_CHAR_BYTES + 2] = 'H';
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
	// separator row
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

		frame[SCOPE_INFO_ROW * SCOPE_STRIDE + SCOPE_INFO_VOICE_POS] = '0';
		frame[SCOPE_INFO_ROW * SCOPE_STRIDE + SCOPE_SEQ_LABEL_POS + 0] = 'S';
		frame[SCOPE_INFO_ROW * SCOPE_STRIDE + SCOPE_SEQ_LABEL_POS + 1] = ':';
		frame[SCOPE_INFO_ROW * SCOPE_STRIDE + SCOPE_SEQ_DIGIT_POS] = '0';
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

	scope_clear_wave_rows(text);
	scope_restore_static_rows(text);
	scope_write_home_suffix(SCOPE_FRAME_BUILD_PTR);

	for (uint8_t col = 0; col < SCOPE_COLS_USED; ++col) {
		uint16_t sample_idx = SCOPE_SAMPLE_INDEX_FROM_COL(col);
		uint16_t dac = scope_get_sample(sample_idx);
		uint8_t new_row = WAVE_ROW_FROM_DAC(dac);

		text[new_row * SCOPE_STRIDE + col] = SCOPE_CHAR_TRACE;
		scope_prev_row[col] = new_row;
	}

	scope_publish_frame();
}