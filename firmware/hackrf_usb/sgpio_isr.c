/*
 * Copyright 2012 Jared Boone
 * Copyright 2013 Benjamin Vernoux
 *
 * This file is part of HackRF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "stdlib.h"
#include "sgpio_isr.h"

#include <libopencm3/lpc43xx/sgpio.h>

#include "usb_bulk_buffer.h"
#include "bluehack.h"
#include "hackrf_core.h"

void sgpio_isr_rx() {
	SGPIO_CLR_STATUS_1 = (1 << SGPIO_SLICE_A);
	sample_counter_rx += 32;

	// buffer full -> ignore samples
	const int64_t sample_diff = sample_pos_write - sample_pos_read;

	if (sample_diff >= SAMPLE_BUFFER_SIZE - 8) {
		sample_pos_read_miss += 8;
		sgpio_callback_finished = 1;
		return;
	}

	const uint32_t index_write = (sample_pos_write & usb_bulk_buffer_mask);
	uint16_t* const p = (uint16_t*)(usb_bulk_buffer + index_write);
	int8_t* const p2 = (int8_t*)p;


	__asm__(
		"ldr r0, [%[SGPIO_REG_SS], #44]\n\t"
		"strh r0, [%[p], #0]\n\t"
		"ldr r0, [%[SGPIO_REG_SS], #32]\n\t"
		"strh r0, [%[p], #6]\n\t"
		:
		: [SGPIO_REG_SS] "l" (SGPIO_PORT_BASE + 0x100),
		  [p] "l" (p)
		: "r0"
	);

	if ((abs(p2[0]) + abs(p2[1])) > 8 || (abs(p2[6]) + abs(p2[7])) > 8) {
		__asm__(
			"ldr r0, [%[SGPIO_REG_SS], #40]\n\t"
			"strh r0, [%[p], #2]\n\t"
			"ldr r0, [%[SGPIO_REG_SS], #36]\n\t"
			"strh r0, [%[p], #4]\n\t"
			: /* no output bind to var */
			: /* input bind to var */
			  [SGPIO_REG_SS] "l" (SGPIO_PORT_BASE + 0x100),
			  [p] "l" (p)
			: /* what has changed */ "r0"
		);

		sample_pos_write += 8;
	} else {
		// no packets left to read
		if ((SAMPLE_BUFFER_SIZE - index_write) < PACKET_SAMPLES_MAX) {
			sample_pos_write += SAMPLE_BUFFER_SIZE - (sample_pos_write & usb_bulk_buffer_mask);
		}
	}
	sgpio_callback_finished = 1;
}

void sgpio_isr_tx() {
	SGPIO_CLR_STATUS_1 = (1 << SGPIO_SLICE_A);
}
