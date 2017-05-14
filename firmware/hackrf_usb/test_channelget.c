#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "bluehack.h"

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


int main(void) {
	BTLE_CONFIG config;
	//uint8_t map[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	//uint8_t map[5] = {0x0F, 0xFF, 0xFF, 0xFF, 0x1F};
	// 00 FC FF FF 1F
	uint8_t map[5] = {0x00, 0xFC, 0xFF, 0xFF, 0x1F};
	// FFFFFFFF1F
	//uint8_t map[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0x1F};
	int i = 0;

	get_channels(&config, map);

	for (; i<38; ++i){
		printf("%d ", config.channel_sequence_data[i]);
	}

	printf("\n");
}
