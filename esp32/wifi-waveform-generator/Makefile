# Build and test the ESP32 Wi-Fi waveform generator.
#
#   make              compile the firmware with arduino-cli (ESP32 Arduino core 3.x)
#   make upload PORT=COM5
#   make test         host unit tests of the DDS core (any C compiler)
#   make dump         sample-dump tool used by docs/generate_figures.py
#   make clean
#
# Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

CLI     ?= arduino-cli
FQBN    ?= esp32:esp32:esp32
PORT    ?= COM5
BUILD   := build
HOST_CC ?= cc
CORE    := $(wildcard wavegen/src/core/*.c)

.PHONY: all upload test dump clean
all:
	$(CLI) compile -b $(FQBN) --warnings all --build-path $(BUILD)/fw wavegen

upload: all
	$(CLI) upload -b $(FQBN) -p $(PORT) --input-dir $(BUILD)/fw wavegen

test: dump
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror tests/test_main.c $(CORE) -lm -o $(BUILD)/host_tests
	$(BUILD)/host_tests

dump:
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tests/dump_wave.c $(CORE) -lm -o $(BUILD)/dump_wave

clean:
	rm -rf $(BUILD)
