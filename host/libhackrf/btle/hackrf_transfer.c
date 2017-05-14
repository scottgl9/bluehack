#include <hackrf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <inttypes.h>

#include "common.h"

static volatile int do_exit = 0;


char *ADV_PDU_TYPE_STR[] = {
	"ADV_IND",
	"ADV_DIRECT_IND",
	"ADV_NONCONN_IND",
	"SCAN_REQ",
	"SCAN_RSP",
	"CONNECT_REQ",
	"ADV_SCAN_IND",
	"RESERVED0",
	"RESERVED1",
	"RESERVED2",
	"RESERVED3",
	"RESERVED4",
	"RESERVED5",
	"RESERVED6",
	"RESERVED7",
	"RESERVED8"
};
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

static struct timeval time_start, time_current;
uint32_t crc_init_internal_adv;
#define DEMOD_BYTES_META_LEN 10

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
        //uint8_t channel_sequence_data_idx; // index for channel_sequence
        uint8_t channel_sequence_data_len; // length of channel_sequence
        int8_t next_channel_trigger;
        uint32_t windowsize_samples;
} BTLE_CONFIG;
BTLE_CONFIG btle_config;

void get_channels(BTLE_CONFIG* config, uint8_t* channelmap) {
        static uint16_t i1, i2;
        static uint8_t channel, used_len, unused_idx_r, unused_idx_w;
        i1 = 0;
        channel = 0;
        used_len = 0;
        unused_idx_r = 0;
        unused_idx_w = 0;
        uint8_t* used_channels[38] = {NULL};
        uint8_t unused_channels[38] = {0};

        for (; i1 < 5; ++i1) {
                i2 = 1;
                //printf("byte %d bit %d\n", i1, i2);

                for (; i2 <= 128; i2*=2) {
                        //printf("bit %d\n", i2);

                        if ((channelmap[i1] & i2) != 0) {
                                // index == channel number
                                config->channel_sequence_data[ channel ] = channel;
                                used_channels[used_len] = &(config->channel_sequence_data[ channel ]);
                                ++used_len;

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
                }
        }

        // unused channels at the end
        for ( ;unused_idx_r < unused_idx_w; ++unused_idx_r) {
                config->channel_sequence_data[ unused_channels[unused_idx_r] ] = *used_channels[ unused_channels[unused_idx_r] % used_len ];
        }
        printf("active channels: %d\n", used_len);
}

// Print packet info to console and optionally save it to file.
void dump_packet(uint8_t channel, uint8_t *packet, uint32_t packetlen, int crc_fail, uint64_t signal_strength) {
	int i = 0;
	//print_hex(packet, packetlen);
	//printf("\n");
	printf("%02d ", channel);
	printf("%-15s crc=%d sig=%6"PRIu64"/%d len=%d",
		ADV_PDU_TYPE_STR[packet[4] & 0x0F],
		crc_fail,
		signal_strength, 255,
		packetlen);

	gettimeofday(&time_current, NULL);
	printf(" t=%7"PRIu64, timediff_useconds(&time_current, &time_start));
	time_start = time_current;

	if (channel < 37) {
		packet[4] = 0xFF;
	}

	switch(packet[4] & 0x0F){
		case ADV_IND:
			//printf(" AdvA=%02X%02X%02X%02X%02X%02X content=", packet[11],packet[10],packet[9],packet[8],packet[7],packet[6]);
			printf(" AdvA=\\x%02X\\x%02X\\x%02X\\x%02X\\x%02X\\x%02X content=",
				packet[6],packet[7],packet[8],packet[9],packet[10],packet[11]);
			print_printable(packet, packetlen);
		break;
		case ADV_NONCONN_IND:
			printf(" AdvA=%02X%02X%02X%02X%02X%02X content=", packet[11],packet[10],packet[9],packet[8],packet[7],packet[6]);
			print_printable(packet, packetlen);
		break;
		case SCAN_REQ:
			printf(" ScanA=%02X%02X%02X%02X%02X%02X AdvA=%02X%02X%02X%02X%02X%02X",
				packet[11],packet[10], packet[9],packet[8],packet[7],packet[6],
				packet[17],packet[16], packet[15],packet[14],packet[13],packet[12]
				);
		break;
		case SCAN_RSP:
			printf(" AdvA=%02X%02X%02X%02X%02X%02X ",
				packet[11],packet[10], packet[9],packet[8],packet[7],packet[6]);
			printf(" content=");
			print_printable(packet, packetlen);
		break;
		case CONNECT_REQ:
			printf(" IA=%02X%02X%02X%02X%02X%02X AdvA=%02X%02X%02X%02X%02X%02X AA=%02X%02X%02X%02X Ws=%02X Wo=%02X%02X Int=%02X%02X CM=%02X%02X%02X%02X%02X HOP=%d !!!!!!!!!!!!!!",
				packet[11], packet[10], packet[9], packet[8], packet[7], packet[6],
				packet[17], packet[16], packet[15], packet[14], packet[13], packet[12],
				packet[21], packet[20], packet[19], packet[18],
				packet[25],
				packet[26], packet[27],
				packet[28], packet[29],
				packet[34], packet[35], packet[36], packet[37], packet[38],
				//(packet[39] & 0xF8) >> 3
				(packet[39] & 0x1F)
				);
			get_channels(&btle_config, packet+34);
			printf("\n");

		        for (; i<38; ++i){
               			printf("%d ", btle_config.channel_sequence_data[i]);
		        }
		break;
		default:
			printf(" DATA? AA=%02X%02X%02X%02X ",
				packet[3], packet[2], packet[1], packet[0]
				);
		break;
	}

	printf("\n");
}


int rx_callback(hackrf_transfer* transfer) {
	uint8_t* buf = transfer->buffer;
	int off = 0;
	int crc_valid = crc_check(buf + 4,
			transfer->valid_length - DEMOD_BYTES_META_LEN - 4 - 3,
                        crc_init_internal_adv);
	off = transfer->valid_length - DEMOD_BYTES_META_LEN + 2;
	printf("%02X%02X%02X%02X%02X%02X%02X%02X [ ",
		buf[off], buf[off+1], buf[off+2], buf[off+3], buf[off+4], buf[off+5], buf[off+6], buf[off+7]);
	dump_packet(buf[transfer->valid_length - DEMOD_BYTES_META_LEN],
		transfer->buffer,
		transfer->valid_length - DEMOD_BYTES_META_LEN,
		crc_valid,
		buf[transfer->valid_length - DEMOD_BYTES_META_LEN + 1]);
	return 0;
	// transfer->buffer

	printf("%d bytes: ", transfer->valid_length);

	for (; off < transfer->valid_length - DEMOD_BYTES_META_LEN; ++off) {
		printf("%X", buf[off]);
	}

	printf(" ch: %d ", buf[off++]);
	printf("sig: %d ", buf[off++]);
	printf("sample: %X %X %X %X %X %X %X %X",
		buf[off], buf[off+1], buf[off+2], buf[off+3], buf[off+4], buf[off+5], buf[off+6], buf[off+7]);
	printf("\n");
	return 0;
}

void sigint_callback_handler(int signum) {
	fprintf(stdout, "Caught signal %d\n", signum);
	do_exit = 1;
}

uint64_t get_freq_by_channel(int channel_number) {
	uint64_t freq_hz;
	if ( channel_number == 37 ) {
		freq_hz = 2402000000ull;
	} else if (channel_number == 38) {
		freq_hz = 2426000000ull;
	} else if (channel_number == 39) {
		freq_hz = 2480000000ull;
	} else if (channel_number >=0 && channel_number <= 10 ) {
		freq_hz = 2404000000ull + channel_number*2000000ull;
	} else if (channel_number >=11 && channel_number <= 36 ) {
		freq_hz = 2428000000ull + (channel_number-11)*2000000ull;
	} else {
		freq_hz = 0xffffffffffffffff;
	}
	return freq_hz;
}


// TODO: put this into firmware
int open_config_board(int rx_vgagain, int do_amplify, int tx_vgagain,
		hackrf_device **rf_dev, void *sigint_handler,
		uint32_t samplerate_hz) {
	const char* serial_number = NULL;

	printf("open_config_board(), rx_vgagain/do_amplify/tx_vgagain/samplerate_hz=%d/%d/%d/%d\n",
		rx_vgagain, do_amplify, tx_vgagain, samplerate_hz);
	hackrf_device *device = NULL;

	int result = hackrf_init();

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
		return -1;
	}

	signal(SIGINT, sigint_handler);
	signal(SIGILL, sigint_handler);
	signal(SIGFPE, sigint_handler);
	signal(SIGSEGV, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGABRT, sigint_handler);

	unsigned int freq_hz = get_freq_by_channel(37);

	printf("hackrf_open_by_serial\n");
	result = hackrf_open_by_serial(serial_number, &device);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_open() failed: %s (%d)\n", hackrf_error_name(result), result);
		return -1;
	}

	printf("hackrf_set_freq(): %ulHz\n", freq_hz);
	result = hackrf_set_freq(device, freq_hz);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(result), result);
		return -1;
	}

	// Each channel is 2 MHz apart, 2MHz spectrum = 4 MHz sample rate
	// 4 MHz = 2MHz (* 2 -> IQ)
	// take 8MHz to avoid problems, see https://github.com/mossmann/hackrf/wiki/Tips-and-Tricks
	// BTLE: The data transmitted has a symbol rate of 1 megasymbols per second. (4.2, p2560)
	// -> 8 samples per symbol on 8M sample rate
	printf("hackrf_set_sample_rate()\n");
	result = hackrf_set_sample_rate(device, samplerate_hz);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_sample_rate() failed: %s (%d)\n", hackrf_error_name(result), result);
		return -1;
	}

	//uint32_t bb_filter_width = hackrf_compute_baseband_filter_bw(2000000ul); // 1750000
	uint32_t bb_filter_width = hackrf_compute_baseband_filter_bw(3000000ul); // 2000000
	//uint32_t bb_filter_width = 2000000;
	printf("proposed filter width: %d\n", bb_filter_width);

	// Each channel is 2 MHz apart, take shortly less
	// "the baseband filter in the MAX2837 has a minimum bandwidth of 1.75MHz"
	printf("hackrf_set_baseband_filter_bandwidth()\n");
	result = hackrf_set_baseband_filter_bandwidth(device, bb_filter_width);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_baseband_filter_bandwidth() failed: %s (%d)\n", hackrf_error_name(result), result);
		return -1;
	}

	// A good default setting to start with is RF=0 (off), IF=16, baseband=16.
	// RF ("amp", 0 or 14 dB)
	// don't use the RF gain - use the IF and BB gains
	//do_amplify = 0;

	int MAX_LNA_GAIN = 40;

	if (do_amplify)
		result |= hackrf_set_amp_enable(device, 1);
	// IF ("lna", 0 to 40 dB in 8 dB steps)
	result |= hackrf_set_lna_gain(device, MAX_LNA_GAIN);
	// baseband ("vga", 0 to 62 dB in 2 dB steps)
	if (rx_vgagain >= 0)
		result |= hackrf_set_vga_gain(device, rx_vgagain);

	/* range 0-47 step 1db */
	//printf("hackrf_set_txvga_gain()\n");
	//result = hackrf_set_txvga_gain(device, 47);
	if (tx_vgagain >= 0)
		result |= hackrf_set_txvga_gain(device, tx_vgagain);

	if( result != HACKRF_SUCCESS ) {
		printf("hackrf_set_XXX_gain() failed: %s (%d)\n", hackrf_error_name(result), result);
		return -1;
	}

	printf("setting device\n");
	(*rf_dev) = device;

	return HACKRF_SUCCESS;
}

int main(int argc, char** argv) {
	hackrf_device* device = NULL;
	int result;
	int exit_code = EXIT_SUCCESS;
	crc_init_internal_adv = crc_init_reorder(CRC_INIT_ADV);
	int i;
	int readchar;

	// channel, rx_vgagain, ampl, tx_vgagain, &btle_config_prog.rf_dev, sigint_callback_handler, 4, RX_BUFLEN, SAMPLE_PER_SYMBOL*1000000ul
	//result = open_config_board(20, 1, 0, &device, sigint_callback_handler, 8000000);
	//result = open_config_board(18, 0, 0, &device, sigint_callback_handler, 8000000); // OK
	//result = open_config_board(16, 0, 0, &device, sigint_callback_handler, 8000000);
	result = open_config_board(14, 0, 0, &device, sigint_callback_handler, 8000000); // OK

	if( result != HACKRF_SUCCESS ) {
		fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
		return EXIT_FAILURE;
	}

/**/
	//\xD9\x8B\x2A\x89\x2D\x6A
	//uint8_t adv_addr[6] = "\xD9\x8B\x2A\x89\x2D\x6A";
	//uint8_t adv_addr[6] = "\xEF\x98\xB3\x4E\x68\x6A";
	uint8_t adv_addr[6] = "\xD2\x1B\x83\xDC\x26\x58";
	//uint8_t adv_addr[6] = "";
	//uint8_t adv_addr[6] = "";
	//uint8_t adv_addr[6] = "";
	//uint8_t adv_addr[6] = "";
	//uint8_t adv_addr[6] = "";

	//result |= hackrf_set_btle_mode(device, BTLE_MODE_FIXEDCHANNEL, NULL, rx_callback);
	result = hackrf_set_btle_mode(device, BTLE_MODE_FOLLOW_STATIC, NULL, rx_callback);
	//result = hackrf_set_btle_mode(device, BTLE_MODE_FOLLOW_DYNAMIC, adv_addr, rx_callback);

	i = 0;
	uint8_t channel = 0;

	for (; i<999; ++i) {
		readchar = fgetc(stdin);

		if(do_exit || readchar == 'e')
			break;

		channel = 37 + (i%3);
		//channel = (i%40);
		result = hackrf_set_btle_channel(device, channel);

		if (result != HACKRF_SUCCESS)
			printf("ERROR when setting channel %d\n", 37 + (i%3));
	}
/**/
/*
	// CC B1 1A 8A C6 F5
	//uint8_t adv_addr[6] = "\xF5\xC6\x8A\x1A\xB1\xCC";
	// 78 BD BC 4F AB 3E
	//uint8_t adv_addr[6] = "\x3E\xAB\x4F\xBC\xBD\x78";
	// 74 5D C4 A0 42 9A
	//uint8_t adv_addr[6] = "\x9A\x42\xA0\xC4\x5D\x75";
	// 78 BD BC CF AB 3E
	//uint8_t adv_addr[6] = "\x3E\xAB\xCF\xBC\xBD\x78";

	// 50 A6 68 14 76 0E
	//uint8_t adv_addr[6] = "\x50\xA6\x68\x14\x76\x0E";
	// 5C 3C 2A E3 0E 67
	// 5C 1D 2A E3 0E 66
	// 5C 1D 2A E3 0E 66
	//uint8_t adv_addr[6] = "\xE0\x73\x4D\xF5\x98\x61";
	//uint8_t adv_addr[6] = "\x3A\x3B\x51\xDB\x74\x6B";
	//uint8_t adv_addr[6] = "\x91\x72\x54\x7E\xF9\x7A";
	//uint8_t adv_addr[6] = "\x17\xC0\xAE\x79\x95\x78";
	//uint8_t adv_addr[6] = "\x68\xE2\x62\x2D\x9E\x5B";
	//uint8_t adv_addr[6] = "\x65\x5F\x2F\xEF\xB2\x55";
	//uint8_t adv_addr[6] = "\x5B\x6D\x1A\xEA\x2D\x67";
	uint8_t adv_addr[6] = "\x33\x6B\x2D\x07\x51\x6A";
	//uint8_t adv_addr[6] = "";

	result = hackrf_set_btle_mode(device, BTLE_MODE_FOLLOW_DYNAMIC, adv_addr, rx_callback);
*/
	sleep(9999);

	printf("\n");
	printf("finishing!\n");

	if (result != HACKRF_SUCCESS) {
		printf("WARNING: ERROR when seeting BTLE_MODE_...\n");
		goto label_exit_close;
	}

	sleep(9999);
	goto label_exit_close;

	printf("BTLE_MODE_OFF\n");
	result |= hackrf_set_btle_mode(device, BTLE_MODE_OFF, NULL, rx_callback);

	if( result != HACKRF_SUCCESS ) {
		fprintf(stderr, "hackrf_set_btle_mode failed: %s (%d)\n", hackrf_error_name(result), result);
		return EXIT_FAILURE;
	}

	printf("will now loop...\n");

	// USB timeout will lead to "hackrf_is_streaming() == false"
	while( (hackrf_is_streaming(device) == HACKRF_TRUE) && (do_exit == 0) ) {
		sleep(1);
		printf("main...\n");
	}

	sleep(2);

label_exit_close:
	if(device != NULL) {
		printf("hackrf_set_btle_mode()\n");
		result |= hackrf_set_btle_mode(device, BTLE_MODE_OFF, NULL, rx_callback);
		printf("hackrf_close()\n");
		result |= hackrf_close(device);

		if(result != HACKRF_SUCCESS) {
			fprintf(stderr, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(result), result);
		}

		hackrf_exit();
	}

	return exit_code;
}
