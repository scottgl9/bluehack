If you are using git, the preferred way to install libopencm3 is to use the
submodule:

$ cd ..
$ git submodule init
$ git submodule update
$ cd firmware/libopencm3
$ make


To build and install a standard firmware image for HackRF One:
$ cd hackrf_usb
$ mkdir build
$ cd build
$ export PATH=...:$PATH
$ cmake ..
$ make
$ hackrf_spiflash -w hackrf_usb.bin

If you have a Jawbreaker, add -DBOARD=JAWBREAKER to the cmake command.

dfu-util --device 1fc9:000c --alt 0 --download hackrf_usb.dfu
