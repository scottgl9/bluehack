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

#ifndef __USB_BULK_BUFFER_H__
#define __USB_BULK_BUFFER_H__

#include <stdint.h>

#include "bluehack.h"

//#define SAMPLE_BUFFER_SIZE 32768 
// 2**15, we only have 64k consecutive ram which is mostly used up for other things
//#define SAMPLE_BUFFER_SIZE 38000 // works
//#define SAMPLE_BUFFER_SIZE 39936
#define SAMPLE_BUFFER_SIZE 32768
//#define SAMPLE_PREBUFFER_LEN PACKET_SAMPLES_MAX
//#define SAMPLE_PREBUFFER_LEN 7168
//#define SAMPLE_PREBUFFER_LEN 0
// This must be true: SAMPLE_BUFFER_SIZE_NOPREBUFFER % 32 == 0
//#define SAMPLE_BUFFER_SIZE_NOPREBUFFER SAMPLE_BUFFER_SIZE-SAMPLE_PREBUFFER_LEN
//#define SAMPLE_BUFFER_SIZE_NOPREBUFFER 32768

/* Address of usb_bulk_buffer is set in ldscripts. If you change the name of this
 * variable, it won't be where it needs to be in the processor's address space,
 * unless you also adjust the ldscripts.
 */
extern uint8_t usb_bulk_buffer[SAMPLE_BUFFER_SIZE];
extern const uint32_t usb_bulk_buffer_mask;

extern volatile uint64_t sample_pos_write;
extern volatile uint64_t sample_pos_read;
extern volatile uint32_t sample_pos_read_miss;
extern volatile uint64_t sample_counter_rx;
extern volatile uint8_t sgpio_callback_finished;
#endif/*__USB_BULK_BUFFER_H__*/
