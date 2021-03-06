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

#include "usb_api_btle.h"
#include "usb_api_transceiver.h"

#include <hackrf_core.h>
#include <rom_iap.h>
#include <usb_queue.h>
#include <libopencm3/lpc43xx/wwdt.h>

#include <streaming.h>
#include <usb.h>
#include "usb_endpoint.h"

#include <stddef.h>
#include <string.h>

#include "tuning.h"
#include "bluehack.h"

#include <libopencm3/cm3/vector.h>
#include <libopencm3/lpc43xx/m4/nvic.h>

#include "sgpio_isr.h"
#include "usb_api_transceiver.h"
#include "usb_bulk_buffer.h"

void set_btle_transfer_mode(const transceiver_mode_t new_transceiver_mode) {
	usb_endpoint_disable(&usb_endpoint_bulk_out);
	_transceiver_mode = new_transceiver_mode;

	if (new_transceiver_mode == TRANSCEIVER_MODE_BTLE) {
		led_off(LED3);
		led_on(LED2);
		usb_endpoint_init(&usb_endpoint_bulk_in);
		rf_path_set_direction(&rf_path, RF_PATH_DIRECTION_RX);
		vector_table.irq[NVIC_SGPIO_IRQ] = sgpio_isr_rx;
		si5351c_activate_best_clock_source(&clock_gen);
		baseband_streaming_enable(&sgpio_config);
	}  else {
		baseband_streaming_disable(&sgpio_config);
		usb_endpoint_disable(&usb_endpoint_bulk_in);
		led_off(LED2);
		led_off(LED3);
		rf_path_set_direction(&rf_path, RF_PATH_DIRECTION_OFF);
	}
}

void reset_counter() {
	sample_pos_write += SAMPLE_BUFFER_SIZE - (sample_pos_write & usb_bulk_buffer_mask);
	sample_pos_read += SAMPLE_BUFFER_SIZE - (sample_pos_read & usb_bulk_buffer_mask);
}

/*
BTLE_MODE_FIXEDCHANNEL: set individual channel -> usb_vendor_request_btle_channel
BTLE_MODE_FOLLOW: follow connection w/ or w/o given AdvAddr
*/
usb_request_status_t usb_vendor_request_btle_mode(
	usb_endpoint_t* const endpoint,
	const usb_transfer_stage_t stage)
{
	if (endpoint->setup.value > 3)
		return USB_REQUEST_STATUS_STALL;

	btle_config_global.btle_mode = endpoint->setup.value;

	if (stage == USB_TRANSFER_STAGE_SETUP) {
		// already off, nothing to be done
		if (btle_config_global.btle_mode == BTLE_MODE_OFF && _transceiver_mode == TRANSCEIVER_MODE_OFF) {
			usb_transfer_schedule_ack(endpoint->in);
			return USB_REQUEST_STATUS_OK;
		}
		// mode change is only possible from off state
		if (btle_config_global.btle_mode != BTLE_MODE_OFF && _transceiver_mode != TRANSCEIVER_MODE_OFF)
			return USB_REQUEST_STATUS_STALL;

		btle_config_global.adv_addr_connect_set = 0;
		btle_config_global.current_channel = 37;

		switch(btle_config_global.btle_mode) {
			case BTLE_MODE_OFF:
				set_btle_transfer_mode(TRANSCEIVER_MODE_OFF);
				usb_transfer_schedule_ack(endpoint->in);
				reset_counter();
				init_btle_config(&btle_config_global);
			break;
			case BTLE_MODE_FIXEDCHANNEL:
				set_channel(btle_config_global.current_channel);
				set_btle_transfer_mode(TRANSCEIVER_MODE_BTLE);
				usb_transfer_schedule_ack(endpoint->in);
			break;
			case BTLE_MODE_FOLLOW_STATIC:
				btle_config_global.follow_state = BTLE_FOLLOWSTATE_ADV_LISTENING;
				usb_transfer_schedule_block(endpoint->out,
					btle_config_global.adv_addr_connect, 6, NULL, NULL);
			break;
			case BTLE_MODE_FOLLOW_DYNAMIC:
				btle_config_global.follow_state = BTLE_FOLLOWSTATE_ADV_SYNCH;
				usb_transfer_schedule_block(endpoint->out,
					btle_config_global.adv_addr_connect, 6, NULL, NULL);
			break;
		}
	} else
	if (stage == USB_TRANSFER_STAGE_DATA) {
		usb_transfer_schedule_ack(endpoint->in);

		// only activate btle if we are not already in this mode
		if (_transceiver_mode == TRANSCEIVER_MODE_BTLE)
			return USB_REQUEST_STATUS_OK;

		// BTLE_MODE_FOLLOW_DYNAMIC needs AdvAddr, check now
		if (btle_config_global.btle_mode == BTLE_MODE_FOLLOW_DYNAMIC && endpoint->setup.length != 6)
			return USB_REQUEST_STATUS_STALL;

		if (endpoint->setup.length == 6)
			btle_config_global.adv_addr_connect_set = 1;

		reset_counter();

		// response arrived, now we can activate streaming
		set_btle_transfer_mode(TRANSCEIVER_MODE_BTLE);

		set_freq(get_freq_by_channel(
			btle_config_global.current_channel
		));
		sample_pos_read_miss = 0;
	}

	return USB_REQUEST_STATUS_OK;
}

void set_channel(uint8_t channel) {
	//sample_pos_read = sample_pos_write;
	reset_counter();
	//sample_pos_read_miss = 0;
	btle_config_global.current_channel = channel;
	set_freq( get_freq_by_channel(channel) );
	reset_counter();
}

usb_request_status_t usb_vendor_request_btle_channel(
	usb_endpoint_t* const endpoint,
	const usb_transfer_stage_t stage)
{
	if (stage == USB_TRANSFER_STAGE_SETUP) {
		if (endpoint->setup.value > 39 ||
			_transceiver_mode != TRANSCEIVER_MODE_BTLE)
			return USB_REQUEST_STATUS_STALL;

		// channel was manually set -> reset state
		switch(btle_config_global.btle_mode) {
			case BTLE_MODE_FOLLOW_STATIC:
				btle_config_global.follow_state = BTLE_FOLLOWSTATE_ADV_LISTENING;
			break;
			case BTLE_MODE_FOLLOW_DYNAMIC:
				btle_config_global.follow_state = BTLE_FOLLOWSTATE_ADV_SYNCH;
			break;
		}

		set_channel(endpoint->setup.value);
		usb_transfer_schedule_ack(endpoint->in);
	}

	return USB_REQUEST_STATUS_OK;
}

usb_request_status_t usb_vendor_request_btle_noisegate(
	usb_endpoint_t* const endpoint,
	const usb_transfer_stage_t stage)
{
	if (stage == USB_TRANSFER_STAGE_SETUP) {
		btle_config_global.noisegate = endpoint->setup.value;
		usb_transfer_schedule_ack(endpoint->in);
	}

	return USB_REQUEST_STATUS_OK;
}
