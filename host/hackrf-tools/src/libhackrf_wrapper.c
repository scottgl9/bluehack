#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <hackrf.h>
#include "libhackrf_wrapper.h"

int32_t rx_callback(hackrf_transfer* transfer) {
	uint32_t idx_write;
	PACKET* packet;

	if (packet_cnt_write - packet_cnt_read > PACKETS_CNT) {
		printf("buffer full, ignoring packet...\n");
		return 0;
	}

	idx_write = packet_cnt_write % PACKETS_CNT;
	packet = &packet_buffer[idx_write];

	// this shouldn't happen, don't overwrite buffer
	if (packet->buffer_len != 0) {
		return 0;
	}

	if (transfer->valid_length > BUFFER_LEN_MAX) {
		printf("USB transfer length > BUFFER_LEN_MAX: %d > %d\n",
			transfer->valid_length, BUFFER_LEN_MAX);
		return 0;
	}

	memcpy(packet->buffer, transfer->buffer, (size_t)transfer->valid_length);
	// WARNING: length should never exceed BUFFER_LEN_MAX
	packet->buffer_len = (uint8_t)transfer->valid_length;
	++packet_cnt_write;

	pthread_mutex_unlock(&lock_packet_read);
	return 0;
}

void sigint_callback_handler(int signum) {
	fprintf(stdout, "Caught signal %d\n", signum);
	do_exit = 1;
	unlock_reader();
}

void unlock_reader() {
	pthread_mutex_unlock(&lock_packet_read);
}

int32_t open_config_board(uint32_t rx_vgagain, int8_t do_amplify, uint32_t tx_vgagain,
		hackrf_device **rf_dev, void *sigint_handler,
		uint32_t samplerate_hz) {
	const char* serial_number = NULL;
	uint32_t freq_hz;
	hackrf_device *device_local = NULL;
	int32_t result;
	uint32_t bb_filter_width;

	printf("open_config_board(), rx_vgagain/do_amplify/tx_vgagain/samplerate_hz=%d/%d/%d/%d\n",
		rx_vgagain, do_amplify, tx_vgagain, samplerate_hz);

	result = hackrf_init();

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	signal(SIGINT, sigint_handler);
	signal(SIGILL, sigint_handler);
	signal(SIGFPE, sigint_handler);
	signal(SIGSEGV, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGABRT, sigint_handler);

	printf("hackrf_open_by_serial\n");
	result = hackrf_open_by_serial(serial_number, &device_local);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_open() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	freq_hz = 2404000000ull; // channel 0

	printf("hackrf_set_freq(): %ulHz\n", freq_hz);
	result = hackrf_set_freq(device_local, freq_hz);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	// Each channel is 2 MHz apart, 2MHz spectrum = 4 MHz sample rate
	// 4 MHz = 2MHz (* 2 -> IQ)
	// take 8MHz to avoid problems, see https://github.com/mossmann/hackrf/wiki/Tips-and-Tricks
	// BTLE: The data transmitted has a symbol rate of 1 megasymbols per second. (4.2, p2560)
	// -> 8 samples per symbol on 8M sample rate
	printf("hackrf_set_sample_rate()\n");
	result = hackrf_set_sample_rate(device_local, samplerate_hz);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_sample_rate() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	bb_filter_width = hackrf_compute_baseband_filter_bw(3000000ul); // 2000000
	printf("proposed filter width: %d\n", bb_filter_width);

	// Each channel is 2 MHz apart, take shortly less
	// "the baseband filter in the MAX2837 has a minimum bandwidth of 1.75MHz"
	printf("hackrf_set_baseband_filter_bandwidth()\n");
	result = hackrf_set_baseband_filter_bandwidth(device_local, bb_filter_width);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_baseband_filter_bandwidth() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	// A good default setting to start with is RF=0 (off), IF=16, baseband=16.
	// RF ("amp", 0 or 14 dB)
	// TODO: disabled, don't use the RF gain - use the IF and BB gains
	//do_amplify = 0;

	if (do_amplify)
		result |= hackrf_set_amp_enable(device_local, 1);
	else
		result |= hackrf_set_amp_enable(device_local, 0);
	// IF ("lna", 0 to 40 dB in 8 dB steps)
	result |= hackrf_set_lna_gain(device_local, MAX_LNA_GAIN);
	// baseband ("vga", 0 to 62 dB in 2 dB steps)
	result |= hackrf_set_vga_gain(device_local, rx_vgagain);

	/* range 0-47 step 1db */
	//printf("hackrf_set_txvga_gain()\n");
	//result = hackrf_set_txvga_gain(device_local, 47);
	result |= hackrf_set_txvga_gain(device_local, tx_vgagain);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_XXX_gain() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	printf("setting device\n");
	(*rf_dev) = device_local;

	return RETURN_OK;
}

int32_t open_board() {
	int32_t result;

	if (device != NULL) {
		return RETURN_OK;
	}

	if (pthread_mutex_init(&lock_packet_read, NULL) != 0) {
		printf("mutex init failed\n");
	}

	pthread_mutex_lock(&lock_packet_read);

	result = open_config_board(20, 1, 0, &device, sigint_callback_handler, 8000000);

	if (result != RETURN_OK) {
		fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

int32_t close_board() {
	int32_t result;

	if (device == NULL) {
		return RETURN_OK;
	}

	result = hackrf_set_btle_mode(device, BTLE_MODE_OFF, NULL, NULL);

	if(result == HACKRF_SUCCESS) {
		result |= hackrf_close(device);
	} else
	if (result == HACKRF_ERROR_INVALID_PARAM) {
		// mode was not changed
		return RETURN_OK;
	} else {
		fprintf(stderr, "hackrf_set_btle_mode() failed: %s (%d)\n", hackrf_error_name(result), result);
		return RETURN_ERROR;
	}

	pthread_mutex_destroy(&lock_packet_read);
	result |= hackrf_exit();

	if (result != HACKRF_SUCCESS) {
		return RETURN_ERROR;
	}

	device = NULL;
	return RETURN_OK;
}

int32_t set_btle_mode(int8_t mode, uint8_t* adv_addr) {
	int32_t result;

	if (device == NULL) {
		return RETURN_ERROR;
	}

	// TODO: check if adv_addr can be NULL?
	result = hackrf_set_btle_mode(device, mode, adv_addr, (hackrf_sample_block_cb_fn)rx_callback);

	if (result != HACKRF_SUCCESS) {
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

int32_t set_noisegate(uint8_t noisegate) {
	int32_t result;

	if (device == NULL) {
		return RETURN_ERROR;
	}

	result = hackrf_set_btle_noisegate(device, noisegate);

	if (result != HACKRF_SUCCESS) {
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

int32_t set_btle_channel(int8_t channel) {
	int32_t result;

	if (device == NULL) {
		return RETURN_ERROR;
	}

	result = hackrf_set_btle_channel(device, channel);

	if (result != HACKRF_SUCCESS) {
		return RETURN_ERROR;
	}

	return RETURN_OK;
}


int32_t get_next_packet(uint8_t* packet, uint8_t* packet_len) {
	uint32_t idx_read;
	PACKET* packet_local;

	if (!packet)
		return RETURN_ERROR;

	*packet_len = 0;

	while (*packet_len == 0 && !do_exit) {
		// TODO: check for blocking if there are no packets at all
		while (packet_cnt_read >= packet_cnt_write) {
			pthread_mutex_lock(&lock_packet_read);

			if (do_exit == 1) {
				return RETURN_ERROR;
			}
		}

		idx_read = packet_cnt_read % PACKETS_CNT;
		packet_local = &packet_buffer[idx_read];

		// this shouldn't happen
		if (packet_local->buffer_len == 0) {
			printf("WARNING: received packet was length 0\n");
			++packet_cnt_read;
			continue;
		}

		// USB timeout will lead to "hackrf_is_streaming() == false"
		if( (hackrf_is_streaming(device) != HACKRF_TRUE)) {
			printf("WARNING: not streaming anymore\n");
		}

		memcpy(packet, packet_local->buffer, packet_local->buffer_len);
		*packet_len = packet_local->buffer_len;
		packet_local->buffer_len = 0;

		++packet_cnt_read;
		break;
	}

	return RETURN_OK;
}
