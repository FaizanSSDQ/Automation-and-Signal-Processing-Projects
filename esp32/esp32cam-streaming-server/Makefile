# Build and test the ESP32-CAM streaming server.
#
#   make              compile the firmware with arduino-cli (ESP32 Arduino core 3.x)
#   make upload PORT=COM5
#   make test         host unit tests of the portable core (any C compiler)
#   make clean
#
# Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

CLI     ?= arduino-cli
FQBN    ?= esp32:esp32:esp32cam
PORT    ?= COM5
BUILD   := build
HOST_CC ?= cc
CORE    := $(wildcard camera_server/src/core/*.c)

.PHONY: all upload test clean
all:
	$(CLI) compile -b $(FQBN) --warnings all --build-path $(BUILD)/fw camera_server

upload: all
	$(CLI) upload -b $(FQBN) -p $(PORT) --input-dir $(BUILD)/fw camera_server

test:
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror tests/test_main.c $(CORE) -lm -o $(BUILD)/host_tests
	$(BUILD)/host_tests

clean:
	rm -rf $(BUILD)
