#include <hackrf.h>

#ifndef LIBHACKRFWRAPPER_H
#define LIBHACKRFWRAPPER_H

pthread_mutex_t lock_packet_read;

#define BUFFER_LEN_MAX 128
#define PACKETS_CNT 1024

#define RETURN_OK 0
#define RETURN_ERROR 1

#define MAX_LNA_GAIN 40

typedef struct {
        uint8_t buffer[BUFFER_LEN_MAX];
        uint8_t buffer_len;
} PACKET;

static hackrf_device* device = NULL;
static volatile int8_t do_exit = 0;

static PACKET packet_buffer[PACKETS_CNT];
static volatile uint64_t packet_cnt_write = 0;
static volatile uint64_t packet_cnt_read = 0;


int32_t rx_callback(hackrf_transfer* transfer);
int32_t get_next_packet(uint8_t* packet, uint8_t* packet_len);
void sigint_callback_handler(int signum);
void unlock_reader(void);
int32_t open_config_board(uint32_t rx_vgagain, int8_t do_amplify, uint32_t tx_vgagain,
		hackrf_device **rf_dev, void *sigint_handler,
		uint32_t samplerate_hz);
int32_t open_board(void);
int32_t close_board(void);
int32_t set_btle_mode(int8_t mode, uint8_t* adv_addr);
int32_t set_btle_channel(int8_t channel);
#endif
