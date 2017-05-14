/*
 * Copyright (c) 2017, Michael Stahn <michael.stahn.42@gmail.com>
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
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#include <hackrf_core.h>

#include "usb_api_btle.h"
#include "usb_bulk_buffer.h"
#include "bluehack.h"
#include "scramble_table.h"
#include "tuning.h"

BTLE_CONFIG btle_config_global;

void init_btle_config(BTLE_CONFIG *config) {
	config->current_channel = CHANNEL_DEFAULT;
	config->btle_mode = BTLE_MODE_OFF;
	config->adv_addr_connect_set = 0;
	memcpy(config->access_addr, "\xD6\xBE\x89\x8E", 4);
	memcpy(config->access_addr_ADV, "\xD6\xBE\x89\x8E", 4);
	config->access_addr_check = 1;
	//config->demod_filter_type = CONNECT_REQ;
	config->noisegate = NOISEGATE_DEFAULT;
	config->is_calc_signal_strength = 1;
	config->next_channel_trigger = -1;
	config->channel_sequence_adv_idx = 0;
	config->channel_sequence_adv[0] = 37;
	config->channel_sequence_adv[1] = 38;
	config->channel_sequence_adv[2] = 39;
	config->channel_sequence_adv_len = 3;
	config->follow_state = BTLE_FOLLOWSTATE_ADV_LISTENING;
	config->channel_sequence_data_idx = 0;
	config->channel_sequence_data_len = 0;
	config->channel_packet_cnt = 0;
}

void blink_led(int led, int delay_rounds) {
			led_off(led);
			delay(delay_rounds);
			led_on(led);
			delay(delay_rounds);
}

void demod(IQ_TYPE* rxp, uint32_t num_byte, uint8_t *out_byte) {
	static uint32_t i, j;
	static uint32_t sample_idx = 0;
	static int I0, Q0, I1, Q1;
	sample_idx = 0;
	i = 0;

	for (; i<num_byte; i++) {
		out_byte[i] = 0;
		j=0;

		// 1 Bit = 1 Symbol = x Samples, x Samples per bit = x times IQ
		for (; j<8; j++) {
			I0 = rxp[sample_idx];
			Q0 = rxp[sample_idx + 1];
			I1 = rxp[sample_idx + 2];
			Q1 = rxp[sample_idx + 3];

			if ((I0*Q1 - I1*Q0) > 0)
				out_byte[i] |= (1 << j);
			sample_idx += SAMPLE_PER_SYMBOL*2;
		}
	}
}

#define ERRORS_MAX 1

uint32_t demod_buffer(IQ_TYPE *sample_buf, uint32_t sample_buf_len, uint8_t channel, BTLE_CONFIG *btle_config, DEMOD_INFO *demod_info) {
	//printf("---> next buffer, len=%d\n", sample_buf_len);
	uint32_t i;
	uint16_t i2;
	uint16_t i3;
	static uint64_t signal_strength;
	static uint8_t packet_len_bytes;
	static uint16_t packet_len_samples;
	static uint8_t len_mask;
	static IQ_TYPE *p = NULL;
	static uint8_t *demod_bytes = NULL;

	demod_info->demod_bytes_len = 0;
	demod_info->warning = WARNING_NONE;
	p = sample_buf;

	for(i=0; i < sample_buf_len; i+=2) {
		while (i < sample_buf_len) {
			// avoid "0*x = 0"
			if ((abs(p[i]) + abs(p[i+1])) >= btle_config->noisegate) {
				break;
			}
			// signal still too low
			i += 2;
		}

		if (i >= sample_buf_len) {
			return sample_buf_len;
		}

		if (i + (1 + BTLE_AA_LEN + BTLE_HDR_LEN + DEMOD_BYTES_MIN)*8*SAMPLE_PER_SYMBOL*2 >= sample_buf_len) {
			demod_info->warning = WARNING_NOT_ENOUGH_SAMPLES;
			return i;
		}

		demod_bytes = demod_info->demod_bytes;
		demod(p + i, 1, demod_bytes);

		// preamble is 0xAA or 0x55 based on AA last bit, don't care which one: we check both
		/*
		v4.2, page 2581: The data channel packet preamble is either 10101010b or 01010101b,
		depending on the LSB of the Access Address. If the LSB of the Access Address is 1,
		the preamble shall be 01010101b, otherwise the preamble shall be 10101010b
		*/
		if (demod_bytes[0] != 0xAA && demod_bytes[0] != 0x55) {
			continue;
		}

		// skip preamble
		i += 8*SAMPLE_PER_SYMBOL*2;
		i3 = 0;

		for(i2=0; i2 < BTLE_AA_LEN; ++i2) {
			// this overwrite preamble previously demodulated
			demod(p + i + i2*8*SAMPLE_PER_SYMBOL*2, 1, demod_bytes + i2);

			if ((demod_bytes[i2] ^ btle_config->access_addr[i2]) != 0) {
				i3 += 1;

				if (i3 > ERRORS_MAX)
					break;
			}
		}

		// no match
		if (i3 > ERRORS_MAX) {
			continue;
		}

		// read header
		demod(p + i + BTLE_AA_LEN * 8*SAMPLE_PER_SYMBOL*2,
				BTLE_HDR_LEN,
				demod_bytes + BTLE_AA_LEN);

		// (PRE:1 +) AA:4 + {[header:2 + data:x] + CRC:3}, {} = scrambled, [] = crc input
		// mask: Adv = 0x3F, data = 0x1F
		len_mask =  channel < 37 ? 0x1F : 0x3F;
		packet_len_bytes = BTLE_AA_LEN + BTLE_HDR_LEN + ((demod_bytes[5] ^ scramble_table[channel][1]) & len_mask) + BTLE_CRC_LEN;


		if (packet_len_bytes > DEMOD_BYTES_MAX) {
			packet_len_bytes = DEMOD_BYTES_MAX;
		}

		if (i + packet_len_bytes*8*SAMPLE_PER_SYMBOL*2 >= sample_buf_len) {
			demod_info->warning = WARNING_NOT_ENOUGH_SAMPLES;
			return i - 8*SAMPLE_PER_SYMBOL*2;
		}

		// read data
		demod(p + i + (BTLE_AA_LEN + BTLE_HDR_LEN)*8*SAMPLE_PER_SYMBOL*2,
				packet_len_bytes - (BTLE_AA_LEN + BTLE_HDR_LEN),
				demod_bytes + (BTLE_AA_LEN + BTLE_HDR_LEN));

		// de-whiten header + data + crc
		i2 = 0;

		for(i2=0; i2 < packet_len_bytes - BTLE_AA_LEN; i2++)
			demod_bytes[BTLE_AA_LEN + i2] = demod_bytes[BTLE_AA_LEN + i2] ^ scramble_table[channel][i2];

		// update packet state
		demod_info->demod_bytes_len = packet_len_bytes;

		if (btle_config->is_calc_signal_strength) {
			signal_strength = 0;
			packet_len_samples = packet_len_bytes*8*SAMPLE_PER_SYMBOL*2;
			i2 = i3 = 0;

			// one sample per byte
			for(; i2 < packet_len_samples; i2+=8*SAMPLE_PER_SYMBOL*2, i3+=1) {
				signal_strength += abs(p[i + i2]) + abs(p[i + i2 + 1]);
			}

			// (I+Q) * i3 -> avg is max 255
			signal_strength /= i3;
			// update packet state
			demod_info->signal_strength = signal_strength;
		}

		// skip demodulated bytes
		i += packet_len_bytes*8*SAMPLE_PER_SYMBOL*2;
		// just 1 packet at a time
		return i;
	}

	return sample_buf_len;
}

uint64_t get_freq_by_channel(int channel_number) {
	uint64_t freq_hz;
	// ADV channel
	if ( channel_number == 37 ) {
		freq_hz = 2402000000ull;
	} else if (channel_number == 38) {
		freq_hz = 2426000000ull;
	} else if (channel_number == 39) {
		freq_hz = 2480000000ull;
	// data channel
	} else if (channel_number >=0 && channel_number <= 10 ) {
		freq_hz = 2404000000ull + channel_number*2000000ull;
	} else if (channel_number >=11 && channel_number <= 36 ) {
		freq_hz = 2428000000ull + (channel_number-11)*2000000ull;
	} else {
		// default: channel 37
		freq_hz = 2402000000ull;
	}
	return freq_hz;
}

void get_channels(BTLE_CONFIG* config, uint8_t* channelmap) {
	static uint16_t i1, i2;
	static uint8_t channel, used_idx, unused_idx_r, unused_idx_w;
	i1 = 0;
	channel = 0;
	used_idx = 0;
	unused_idx_r = 0;
	unused_idx_w = 0;
	uint8_t* used_channels[40] = {NULL};
	uint8_t unused_channels[40] = {0};

	for (; i1 < 5; ++i1) {
		i2 = 1;

		for (; i2 <= 128; i2*=2) {
			if ((channelmap[i1] & i2) != 0) {
				// index == channel number
				config->channel_sequence_data[ channel ] = channel;
				used_channels[used_idx] = &(config->channel_sequence_data[ channel ]);
				++used_idx;

				if (unused_idx_r != unused_idx_w) {
					// fill unused channels with next used channel
					config->channel_sequence_data[ unused_channels[unused_idx_r] ] = channel;
					++unused_idx_r;
				}
			} else {
				unused_channels[unused_idx_w] = channel;
				++unused_idx_w;
			}

			++channel;

			if (channel >= 37)
				break;
		}
	}
	// unused channels at the end
	for (; unused_idx_r < unused_idx_w; unused_idx_r++) {
		config->channel_sequence_data[ unused_channels[unused_idx_r] ] = *used_channels[ unused_channels[unused_idx_r] % used_idx ];
	}
}

void update_btle_state(uint8_t* packet, uint8_t packet_len) {
	static uint8_t packet_type = 0;
	packet_type = packet[4] & 0x0F;

	// TODO: this leads to a dead end if there is no traffic
	if (btle_config_global.follow_state == BTLE_FOLLOWSTATE_ADV_SYNCH) {
		if (packet_type == CONNECT_REQ) {
			if (memcmp(packet + 4 + 2 + 6, btle_config_global.adv_addr_connect, 6) != 0) {
				return;
			}

			btle_config_global.follow_state = BTLE_FOLLOWSTATE_ADV_LISTENING;
		} else {
			if (packet_type != ADV_IND && memcmp(packet + 4 + 2, btle_config_global.adv_addr_connect, 6) != 0) {
				return;
			}

			switch (btle_config_global.current_channel) {
				case 37:
					btle_config_global.next_channel_trigger = 38;
				break;
				case 38:
					btle_config_global.next_channel_trigger = 39;
				break;
				case 39:
					btle_config_global.next_channel_trigger = 37;
				break;
			}
			// 150us + pkt + delay
			btle_config_global.sample_pos_store[0] = sample_counter_rx + 1200*2 + 50*8*8*2 + 4000;
			return;
		}
	}
	if (btle_config_global.follow_state == BTLE_FOLLOWSTATE_ADV_LISTENING) {
		if (packet_type != CONNECT_REQ)
			return;

		// only follow specific ADVAddr?
		if (btle_config_global.adv_addr_connect_set == 1 &&
			memcmp(packet + 4 + 2 + 6, btle_config_global.adv_addr_connect, 6) != 0)
			return;

		// plausability check for chanmap, middle freqs are most likely used
		// TODO: improve
		if (packet[36] != 0xFF || packet[37] != 0xFF)
			return;

		btle_config_global.channel_increment = (packet[39] & 0x1F);
		btle_config_global.channel_sequence_data_idx = btle_config_global.channel_increment;

		memcpy(btle_config_global.access_addr, packet + 18, 4);
		get_channels(&btle_config_global, packet+34);

		/*
		transmitWindowOffset = 1.25ms * Interval
		transmitWindowSize = 1.25ms * Window Size

		Therefore the start of the first packet will be no earlier than 1.25 ms +
		transmitWindowOffset and no later than 1.25 ms + transmitWindowOffset +
		transmitWindowSize after the end of the CONNECT_REQ PDU transmitted in
		the advertising channel.

		The lastUnmappedChannel shall be 0 for the first connection
		event of a connection.

		Inter Frame Space = 150us
		ACK = 80us

		Each connection event contains at least one packet sent by the master.

		The connInterval shall be a multiple of 1.25 ms in the range of 7.5 ms to 4.0 s.
		The connInterval is set by the Initiator’s Link Layer in the CONNNECT_REQ
		PDU from the range given by the Host. p2619

		Connection interval: Determines how often the Central will ask for data from the Peripheral. When the Peripheral requests an update, it supplies a maximum and a minimum wanted interval. The connection interval must be between 7.5 ms and 4 s.
		Slave latency: By setting a non-zero slave latency, the Peripheral can choose to not answer when the Central asks for data up to the slave latency number of times. However, if the Peripheral has data to send, it can choose to send data at any time.
		Connection supervision timeout: This timeout determines the timeout from the last data exchange till a link is considered lost. 
		*/
		// assume offset is max 1 byte (offset 26), actually 2
		//btle_config_global.window_offset_samples = packet[25]* (8000*2);
		//btle_config_global.windo_size_samples = packet[26]* (8000*2);
		btle_config_global.conint_samples = packet[28] * (1.25f * 8000.0f * 2.0f);

		btle_config_global.current_channel = btle_config_global.channel_sequence_data[ btle_config_global.channel_sequence_data_idx ];
		btle_config_global.next_channel_trigger = -1;
		btle_config_global.follow_state = BTLE_FOLLOWSTATE_FOLLOWING;

		set_channel(btle_config_global.current_channel);
	} else
	if (btle_config_global.follow_state == BTLE_FOLLOWSTATE_FOLLOWING) {
		++btle_config_global.channel_packet_cnt;

		if (btle_config_global.channel_packet_cnt == 1) {
			btle_config_global.sample_pos_store[0] = sample_counter_rx
				+ btle_config_global.conint_samples
				- 2*(1.25f * 8000.0f * 2.0f)
				- packet_len*8*8*2;
			btle_config_global.next_channel_trigger = btle_config_global.channel_sequence_data[
				(btle_config_global.channel_sequence_data_idx + btle_config_global.channel_increment) % 37
				];
		} else
		if (sample_counter_rx + DEMOD_BYTES_MIN*8*8*2 + 1200 + 12000 > btle_config_global.sample_pos_store[0]) {
			btle_config_global.sample_pos_store[0] = sample_counter_rx
				+ btle_config_global.conint_samples;
			btle_config_global.channel_packet_cnt = 0;
			btle_config_global.channel_sequence_data_idx = (btle_config_global.channel_sequence_data_idx
				+ btle_config_global.channel_increment) % 37;

			btle_config_global.current_channel = btle_config_global.channel_sequence_data[
				btle_config_global.channel_sequence_data_idx ];

			btle_config_global.next_channel_trigger = btle_config_global.channel_sequence_data[
				(btle_config_global.channel_sequence_data_idx + btle_config_global.channel_increment) % 37
				];
			set_channel(btle_config_global.current_channel);
		}
	}
}
