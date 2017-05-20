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
#ifndef BLUEHACK_H
#define BLUEHACK_H

#define SAMPLE_PER_SYMBOL	2
#define NOISEGATE_DEFAULT	8
#define CHANNEL_DEFAULT		39
#define CHANNEL_MAX		39

#define BTLE_PREAMBLE_LEN	1
#define BTLE_AA_LEN		4
#define BTLE_HDR_LEN		2
#define BTLE_CRC_LEN		3

typedef enum {
	// transceiver off
	BTLE_MODE_OFF = 0,
	// only listen on one channel
	BTLE_MODE_FIXEDCHANNEL = 1,
	// listen for CONNECT_REQ on a fixed channel, AdvAddr can optionally be set
	BTLE_MODE_FOLLOW_STATIC = 2,
	// listen for CONNECT_REQ on all ADV channels, needs AdvAddr to be set
	BTLE_MODE_FOLLOW_DYNAMIC = 3
} BTLE_MODE;

typedef enum {
	// synch channels
	BTLE_FOLLOWSTATE_ADV_SYNCH = 0,
	// listening on ADV channels
	BTLE_FOLLOWSTATE_ADV_LISTENING = 1,
	// following active connection
	BTLE_FOLLOWSTATE_FOLLOWING = 2,
} BTLE_FOLLOWSTATE;

typedef struct {
	uint8_t current_channel;
	uint16_t noisegate;
	uint8_t access_addr[4];
	uint8_t access_addr_ADV[4];
	uint8_t access_addr_check; // ignore access addr?
	uint8_t adv_addr_connect[6];
	uint8_t adv_addr_connect_set;
	uint32_t crc_init_internal;
	uint32_t crc_init_internal_ADV;
	int8_t is_crc_check_error;
	int8_t is_calc_signal_strength;
	int8_t btle_mode; // one of BTLE_MODE
	int8_t follow_state; // one of BTLE_FOLLOWSTATE
	int8_t demod_filter_type;
	uint8_t channel_increment;
	uint8_t channel_packet_cnt;
	uint64_t sample_pos_store[3];
	uint8_t channel_sequence_adv[6]; // sequence of adv. channels to scan for CONNECT_REQ
	uint8_t channel_sequence_adv_idx; // index for channel_sequence
	uint8_t channel_sequence_adv_len; // length of channel_sequence
	uint8_t channel_sequence_data[38]; // sequence of adv. channels to scan for CONNECT_REQ
	uint8_t channel_sequence_data_idx; // index for channel_sequence
	uint8_t channel_sequence_data_len; // length of channel_sequence
	int8_t next_channel_trigger;
	uint32_t window_offset_samples;
	uint32_t window_size_samples;
	uint32_t conint_samples;
	uint32_t conint_samples_before;
} BTLE_CONFIG;

extern BTLE_CONFIG btle_config_global;

#define DEMOD_BYTES_MIN 9
#define DEMOD_BYTES_MAX 60
#define DEMOD_BYTES_META_LEN 10

// TODO: rewrite as enum
#define WARNING_NONE			0
#define WARNING_NOT_ENOUGH_SAMPLES	1

typedef struct {
	uint8_t demod_bytes[DEMOD_BYTES_MAX + DEMOD_BYTES_META_LEN];
	uint8_t demod_bytes_len; // amount of demodulated bytes
	int8_t is_crc_valid;
	uint16_t signal_strength;
	int8_t warning; // one of WARNING_...
	uint8_t is_used;
} DEMOD_INFO;

typedef enum {
	LL_CONNECTION_UPDATE_REQ = 0,
	LL_CHANNEL_MAP_REQ = 1,
	LL_TERMINATE_IND = 2,
	LL_ENC_REQ = 3,
	LL_ENC_RSP = 4,
	LL_START_ENC_REQ = 5,
	LL_START_ENC_RSP = 6,
	LL_UNKNOWN_RSP = 7,
	LL_FEATURE_REQ = 8,
	LL_FEATURE_RSP = 9,
	LL_PAUSE_ENC_REQ = 10,
	LL_PAUSE_ENC_RSP = 11,
	LL_VERSION_IND = 12,
	LL_REJECT_IND = 13
} BTLE_LL_CTRL_PDU_PAYLOAD_TYPE;


typedef enum {
	ADV_IND = 0,
	ADV_DIRECT_IND = 1,
	ADV_NONCONN_IND = 2,
	SCAN_REQ = 3,
	SCAN_RSP = 4,
	CONNECT_REQ = 5,
	ADV_SCAN_IND = 6,
	RESERVED0 = 7,
	RESERVED1 = 8,
	RESERVED2 = 9,
	RESERVED3 = 10,
	RESERVED4 = 11,
	RESERVED5 = 12,
	RESERVED6 = 13,
	RESERVED7 = 14,
	RESERVED8 = 15
} BTLE_ADV_PDU_TYPE;

typedef int8_t IQ_TYPE;

#define PACKET_SAMPLES_MIN DEMOD_BYTES_MIN*8*SAMPLE_PER_SYMBOL*2
#define PACKET_SAMPLES_MAX DEMOD_BYTES_MAX*8*SAMPLE_PER_SYMBOL*2
#define PACKET_SAMPLES_CONNECT (1+37)*8*8*2
#define PACKET_SAMPLES_50MS 50*8000*2

#define DELAY_ROUNDS_1 20000000
#define DELAY_ROUNDS_05 10000000 // 0.5 sec
#define DELAY_ROUNDS_02 1000000

void blink_led(int led, int delay_rounds);
void init_btle_config(BTLE_CONFIG *config);
void get_channels(BTLE_CONFIG* config, uint8_t* channelmap);
void demod(IQ_TYPE* rxp, uint32_t num_byte, uint8_t *out_byte);
uint32_t demod_buffer(IQ_TYPE *sample_buf, uint32_t sample_buf_len, uint8_t channel,
	BTLE_CONFIG *btle_config, DEMOD_INFO *demod_info);
uint64_t get_freq_by_channel(int channel_number);
void update_btle_state(uint8_t* buf, uint8_t buf_len);
#endif

