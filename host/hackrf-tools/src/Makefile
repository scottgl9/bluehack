CC=gcc

#CFLAGS+=-g -fPIC -Wall -Wextra -Werror -Wconversion -Wpedantic -Wdeclaration-after-statement -W -Wshadow
#CFLAGS+=-Wcast-qual -Wwrite-strings -Wundef -Wcast-align -Wmissing-prototypes -Wno-missing-field-initializers
CFLAGS+=-shared -fPIC -Wall -Wextra -Werror -Wconversion -Wdeclaration-after-statement -W -Wshadow -Wcast-qual -Wwrite-strings -Wundef -Wcast-align -Wmissing-prototypes -Wno-missing-field-initializers
HACKRF_HEADER=../../libhackrf/src/
HACKRF_LIB_PATH=$(realpath ../../libhackrf/build/src)
HACKRF_LIB=$(HACKRF_LIB_PATH)/libhackrf.so.0

DIR_BUILD=./build

all: libhackrf_wrapper.so

libhackrf_wrapper.so:
	mkdir $(DIR_BUILD)
	$(CC) $(CFLAGS) -I$(HACKRF_HEADER) -L$(HACKRF_LIB_PATH) -Wl,-rpath $(HACKRF_LIB_PATH) libhackrf_wrapper.c  $(HACKRF_LIB) -o $(DIR_BUILD)/$@

.PHONY: clean
clean:
	@rm -rf libhackrf_wrapper.so
	@rm -rf $(DIR_BUILD)
