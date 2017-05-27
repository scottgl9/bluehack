/*
 * Copyright 2012 Jared Boone
 * Copyright 2013 Benjamin Vernoux
 * Copyright 2017 Michael Stahn <michael.stahn.42@gmail.com>
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

#include <stddef.h>
#include <unistd.h>
#include <string.h>

#include <libopencm3/lpc43xx/m4/nvic.h>

#include <streaming.h>

#include "tuning.h"

#include "usb.h"
#include "usb_standard_request.h"

#include <rom_iap.h>
#include "usb_descriptor.h"

#include "usb_device.h"
#include "usb_endpoint.h"
#include "usb_api_board_info.h"
//#include "usb_api_cpld.h"
#include "usb_api_register.h"
#include "usb_api_spiflash.h"
#include "usb_api_operacake.h"
#include "operacake.h"
//#include "usb_api_sweep.h"
#include "usb_api_transceiver.h"
#include "usb_bulk_buffer.h"
#include "usb_api_btle.h"
#include "bluehack.h"

static const usb_request_handler_fn vendor_request_handler[] = {
	NULL,
	usb_vendor_request_set_transceiver_mode,
	usb_vendor_request_write_max2837,
	usb_vendor_request_read_max2837,
	usb_vendor_request_write_si5351c,
	usb_vendor_request_read_si5351c,
	usb_vendor_request_set_sample_rate_frac,
	usb_vendor_request_set_baseband_filter_bandwidth,
	usb_vendor_request_write_rffc5071,
	usb_vendor_request_read_rffc5071,
	usb_vendor_request_erase_spiflash,
	usb_vendor_request_write_spiflash,
	usb_vendor_request_read_spiflash,
	NULL, // used to be write_cpld
	usb_vendor_request_read_board_id,
	usb_vendor_request_read_version_string,
	usb_vendor_request_set_freq,
	usb_vendor_request_set_amp_enable,
	usb_vendor_request_read_partid_serialno,
	usb_vendor_request_set_lna_gain,
	usb_vendor_request_set_vga_gain,
	usb_vendor_request_set_txvga_gain,
	NULL, // was set_if_freq
#ifdef HACKRF_ONE
	usb_vendor_request_set_antenna_enable,
#else
	NULL,
#endif
	usb_vendor_request_set_freq_explicit,
	usb_vendor_request_read_wcid,  // USB_WCID_VENDOR_REQ
	NULL, //usb_vendor_request_init_sweep,
	usb_vendor_request_operacake_get_boards,
	usb_vendor_request_operacake_set_ports,
	usb_vendor_request_set_hw_sync_mode,
	usb_vendor_request_reset,
	usb_vendor_request_btle_mode,
	usb_vendor_request_btle_channel,
	usb_vendor_request_btle_noisegate
};

static const uint32_t vendor_request_handler_count =
	sizeof(vendor_request_handler) / sizeof(vendor_request_handler[0]);

usb_request_status_t usb_vendor_request(
	usb_endpoint_t* const endpoint,
	const usb_transfer_stage_t stage
) {
	usb_request_status_t status = USB_REQUEST_STATUS_STALL;
	
	if( endpoint->setup.request < vendor_request_handler_count ) {
		usb_request_handler_fn handler = vendor_request_handler[endpoint->setup.request];
		if( handler ) {
			status = handler(endpoint, stage);
		}
	}
	
	return status;
}

const usb_request_handlers_t usb_request_handlers = {
	.standard = usb_standard_request,
	.class = 0,
	.vendor = usb_vendor_request,
	.reserved = 0,
};

void usb_configuration_changed(
	usb_device_t* const device
) {
	/* Reset transceiver to idle state until other commands are received */
	_transceiver_mode = TRANSCEIVER_MODE_OFF;

	if( device->configuration->number == 1 ) {
		// transceiver configuration
		cpu_clock_pll1_max_speed();
		led_on(LED1);
	} else {
		/* Configuration number equal 0 means usb bus reset. */
		cpu_clock_pll1_low_speed();
		led_off(LED1);
	}
}

void usb_set_descriptor_by_serial_number(void)
{
	iap_cmd_res_t iap_cmd_res;
	
	/* Read IAP Serial Number Identification */
	iap_cmd_res.cmd_param.command_code = IAP_CMD_READ_SERIAL_NO;
	iap_cmd_call(&iap_cmd_res);
	
	if (iap_cmd_res.status_res.status_ret == CMD_SUCCESS) {
		usb_descriptor_string_serial_number[0] = USB_DESCRIPTOR_STRING_SERIAL_BUF_LEN;
		usb_descriptor_string_serial_number[1] = USB_DESCRIPTOR_TYPE_STRING;
		
		/* 32 characters of serial number, convert to UTF-16LE */
		for (size_t i=0; i<USB_DESCRIPTOR_STRING_SERIAL_LEN; i++) {
			const uint_fast8_t nibble = (iap_cmd_res.status_res.iap_result[i >> 3] >> (28 - (i & 7) * 4)) & 0xf;
			const char c = (nibble > 9) ? ('a' + nibble - 10) : ('0' + nibble);
			usb_descriptor_string_serial_number[2 + i * 2] = c;
			usb_descriptor_string_serial_number[3 + i * 2] = 0x00;
		}
	} else {
		usb_descriptor_string_serial_number[0] = 2;
		usb_descriptor_string_serial_number[1] = USB_DESCRIPTOR_TYPE_STRING;
	}
}

#define DEMOD_INFO_CNT 100
#define DEMOD_INFO_CNT_MASK DEMOD_INFO_CNT-1
DEMOD_INFO demod_infos[DEMOD_INFO_CNT];

void completion_callback(void* user_data, unsigned int len __attribute__((unused))) {
	((DEMOD_INFO*)user_data)->is_used = 0;
}


int main(void) {
	pin_setup();
	enable_1v8_power();
#ifdef HACKRF_ONE
	enable_rf_power();
#endif
	cpu_clock_init();

	usb_set_descriptor_by_serial_number();

	usb_set_configuration_changed_cb(usb_configuration_changed);
	usb_peripheral_reset();

	usb_device_init(0, &usb_device);

	usb_queue_init(&usb_endpoint_control_out_queue);
	usb_queue_init(&usb_endpoint_control_in_queue);
	usb_queue_init(&usb_endpoint_bulk_out_queue);
	usb_queue_init(&usb_endpoint_bulk_in_queue);

	usb_endpoint_init(&usb_endpoint_control_out);
	usb_endpoint_init(&usb_endpoint_control_in);

	nvic_set_priority(NVIC_USB0_IRQ, 0xFF);

	usb_run(&usb_device);

	rf_path_init(&rf_path);
	// TODO: needed?
	//operacake_init();

	static uint32_t buflen_to_read = 0;
	static uint32_t bytesread = 0;
	static uint32_t idx_write = 0;
	static uint32_t idx_read = 0;
	static uint8_t demod_info_idx = 0;
	static uint8_t demod_len_tmp = 0;
	static uint64_t sample_counter_rx_tmp = 0;
	static uint8_t channel_tmp = 0;
	static DEMOD_INFO* demod_info_ptr = NULL;

	for (; demod_info_idx < DEMOD_INFO_CNT; ++demod_info_idx)
		demod_infos[demod_info_idx].is_used = 0;

	demod_info_idx = 0;

	init_btle_config(&btle_config_global);

	while(true) {
		if (_transceiver_mode == TRANSCEIVER_MODE_BTLE) {
			while (!sgpio_callback_finished) { }

			sgpio_callback_finished = 0;

			if (btle_config_global.next_channel_trigger != -1 &&
					(sample_counter_rx > btle_config_global.sample_pos_store[0])) {

					if (btle_config_global.follow_state != BTLE_FOLLOWSTATE_ADV_SYNCH) {
						btle_config_global.channel_packet_cnt = 0;
						set_channel(btle_config_global.next_channel_trigger);
						btle_config_global.channel_sequence_data_idx = (btle_config_global.channel_sequence_data_idx + btle_config_global.channel_increment) % 37;
						btle_config_global.next_channel_trigger = -1;
					} else {
						set_channel(btle_config_global.next_channel_trigger);
						btle_config_global.channel_sequence_data_idx = (btle_config_global.channel_sequence_data_idx + btle_config_global.channel_increment) % 37;
						btle_config_global.next_channel_trigger = -1;
					}
			}

			// not enough samples, same pos or beyond write = block
			if (sample_pos_read >= sample_pos_write ||
				(sample_pos_write - sample_pos_read) < PACKET_SAMPLES_MIN)
				continue;

			idx_read = sample_pos_read & usb_bulk_buffer_mask;
			idx_write = sample_pos_write & usb_bulk_buffer_mask;

			if (idx_read < idx_write) {
				// write ahead: just read until write
				buflen_to_read = idx_write - idx_read; // start> read | ... | write | ...<end

				// wait until enough space or write wraps
				if (buflen_to_read < PACKET_SAMPLES_MIN) {
					continue;
				}
			} else {
				// write behind: read until end of buffer
				buflen_to_read = SAMPLE_BUFFER_SIZE - idx_read; // // start> write | ... | read | ...<end

				if (buflen_to_read < PACKET_SAMPLES_MAX) {
					sample_pos_read += SAMPLE_BUFFER_SIZE - (sample_pos_read & usb_bulk_buffer_mask);
					continue;
				}
			}

			// remember current channel gives (more) correct results
			// avoids problem like ch: 37 -> demod -> switch channel to 38 -> transmit to host: channel 38 (which is actual 37)
			channel_tmp = btle_config_global.current_channel;
			sample_counter_rx_tmp = sample_counter_rx;

			// all demod infos are used up, wait for completion callback to free one
			while (demod_infos[demod_info_idx].is_used == 1)
				blink_led(LED3, DELAY_ROUNDS_05);

			demod_info_ptr = &(demod_infos[demod_info_idx]);
			demod_info_ptr->is_used = 1;

			// using btle_config_global is okay even if shared: we first have to demod to react on packet content
			bytesread = demod_buffer(((int8_t*)usb_bulk_buffer) + idx_read,
				buflen_to_read,
				channel_tmp,
				&btle_config_global,
				demod_info_ptr
				);

			sample_pos_read += bytesread;

			// "idx_write == idx_read == true" only if read is behind because buflen_to_read > 0
			if (demod_info_ptr->warning == WARNING_NOT_ENOUGH_SAMPLES && idx_write <= idx_read) {
				// no chance to get missing samples: start at 0
				sample_pos_read += SAMPLE_BUFFER_SIZE - (sample_pos_read & usb_bulk_buffer_mask);
				// WARNING_NOT_ENOUGH_SAMPLES means "demod_info_ptr->demod_bytes_len == 0"
			}


			if (demod_info_ptr->demod_bytes_len == 0) {
				// don't change idx, demod info can be reused
				demod_info_ptr->is_used = 0;
				continue;
			}

			demod_len_tmp = demod_info_ptr->demod_bytes_len;
			demod_info_ptr->demod_bytes[demod_len_tmp] = channel_tmp;
			demod_info_ptr->demod_bytes[demod_len_tmp + 1] = demod_info_ptr->signal_strength;

			//demod_info_ptr->demod_bytes[demod_len_tmp + 2] = (sample_pos_read & 0xFF00000000000000) >> 7*8;
			//demod_info_ptr->demod_bytes[demod_len_tmp + 3] = (sample_pos_read & 0x00FF000000000000) >> 6*8;
			//demod_info_ptr->demod_bytes[demod_len_tmp + 4] = (sample_pos_read & 0x0000FF0000000000) >> 5*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 2] = (sample_pos_read_miss & 0xFF0000) >> 2*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 3] = (sample_pos_read_miss & 0x00FF00) >> 1*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 4] = (sample_pos_read_miss & 0x0000FF);


			demod_info_ptr->demod_bytes[demod_len_tmp + 5] = (sample_counter_rx_tmp & 0x000000FF00000000) >> 4*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 6] = (sample_counter_rx_tmp & 0x00000000FF000000) >> 3*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 7] = (sample_counter_rx_tmp & 0x0000000000FF0000) >> 2*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 8] = (sample_counter_rx_tmp & 0x000000000000FF00) >> 1*8;
			demod_info_ptr->demod_bytes[demod_len_tmp + 9] = (sample_counter_rx_tmp & 0x00000000000000FF);
			demod_info_ptr->demod_bytes_len += DEMOD_BYTES_META_LEN;

			usb_transfer_schedule_block(
				&usb_endpoint_bulk_in,
				demod_info_ptr->demod_bytes,
				demod_info_ptr->demod_bytes_len,
				completion_callback,
				demod_info_ptr
				);

			if (btle_config_global.btle_mode != BTLE_MODE_FIXEDCHANNEL)
				update_btle_state(demod_info_ptr->demod_bytes,
					demod_info_ptr->demod_bytes_len);

			demod_info_idx = (demod_info_idx + 1) & (DEMOD_INFO_CNT_MASK);
		} else {
			blink_led(LED1, DELAY_ROUNDS_05);
		}
	}

	return 0;
}
