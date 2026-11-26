# Build and test the ESP-NOW sensor network (one sketch, two roles).
#
#   make              compile the node and the gateway firmware with arduino-cli
#   make upload-node PORT=COM5 NODE=2
#   make upload-gateway PORT=COM6
#   make test         host unit tests of the portable core (any C compiler)
#   make clean
#
# Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

CLI     ?= arduino-cli
FQBN    ?= esp32:esp32:esp32
PORT    ?= COM5
NODE    ?= 1
BUILD   := build
HOST_CC ?= cc
CORE    := $(wildcard sensor_network/src/core/*.c)
GW_FLAGS = --build-property "compiler.cpp.extra_flags=-DNET_ROLE=1" --build-property "compiler.c.extra_flags=-DNET_ROLE=1"
NODE_FLAGS = --build-property "compiler.cpp.extra_flags=-DNODE_ID=$(NODE)" --build-property "compiler.c.extra_flags=-DNODE_ID=$(NODE)"

.PHONY: all node gateway upload-node upload-gateway test clean
all: node gateway

node:
	$(CLI) compile -b $(FQBN) --warnings all $(NODE_FLAGS) --build-path $(BUILD)/node sensor_network

gateway:
	$(CLI) compile -b $(FQBN) --warnings all $(GW_FLAGS) --build-path $(BUILD)/gateway sensor_network

upload-node: node
	$(CLI) upload -b $(FQBN) -p $(PORT) --input-dir $(BUILD)/node sensor_network

upload-gateway: gateway
	$(CLI) upload -b $(FQBN) -p $(PORT) --input-dir $(BUILD)/gateway sensor_network

test:
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror tests/test_main.c $(CORE) -o $(BUILD)/host_tests
	$(BUILD)/host_tests

clean:
	rm -rf $(BUILD)
