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

#include "usb_bulk_buffer.h"

const uint32_t usb_bulk_buffer_mask = SAMPLE_BUFFER_SIZE - 1;

volatile uint64_t sample_pos_write = 0;
volatile uint64_t sample_pos_read = 0;
volatile uint32_t sample_pos_read_miss = 0;
volatile uint64_t sample_counter_rx = 0;
volatile uint8_t sgpio_callback_finished = 0;
