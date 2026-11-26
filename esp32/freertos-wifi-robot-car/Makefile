# Build and test the ESP32 Wi-Fi robot car.
#
#   make              compile the firmware with arduino-cli (ESP32 Arduino core 3.x)
#   make upload PORT=COM5
#   make test         host unit tests of the portable core (any C compiler)
#   make sim          host simulation used by the README figures
#   make clean
#
# Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

CLI     ?= arduino-cli
FQBN    ?= esp32:esp32:esp32
PORT    ?= COM5
BUILD   := build
HOST_CC ?= cc
CORE    := $(wildcard robot_car/src/core/*.c)

.PHONY: all upload test sim clean
all:
	$(CLI) compile -b $(FQBN) --warnings all --build-path $(BUILD)/fw robot_car

upload: all
	$(CLI) upload -b $(FQBN) -p $(PORT) --input-dir $(BUILD)/fw robot_car

test: sim
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror tests/test_main.c $(CORE) -lm -o $(BUILD)/host_tests
	$(BUILD)/host_tests

sim:
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tests/sim_drive.c $(CORE) -lm -o $(BUILD)/sim_drive

clean:
	rm -rf $(BUILD)
