# Standalone build for the NUCLEO-F103RB SUMP logic analyzer.
# Works with any arm-none-eabi-gcc (STM32CubeIDE bundles one; CI uses the Ubuntu package).
#
#   make            build build/logic_analyzer.elf/.hex/.bin
#   make test       run host unit tests for the portable modules (also builds the simulator)
#   make sim        build the host simulator used by the Python tests and figures
#   make clean
#
# Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

TARGET   := logic_analyzer
BUILD    := build
PREFIX   ?= arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
SIZE     := $(PREFIX)size
HOST_CC  ?= cc

MCU      := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFS     := -DUSE_HAL_DRIVER -DSTM32F103xB
INCS     := -ICore/Inc \
            -IDrivers/STM32F1xx_HAL_Driver/Inc \
            -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
            -IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
            -IDrivers/CMSIS/Include

CFLAGS   := $(MCU) $(DEFS) $(INCS) -std=gnu11 -O2 -g3 -Wall -ffunction-sections -fdata-sections
LDFLAGS  := $(MCU) -TSTM32F103RBTX_FLASH.ld --specs=nano.specs --specs=nosys.specs \
            -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

APP_SRC  := $(wildcard Core/Src/*.c)
HAL_SRC  := $(wildcard Drivers/STM32F1xx_HAL_Driver/Src/*.c)
ASM_SRC  := Core/Startup/startup_stm32f103rbtx.s
OBJS     := $(addprefix $(BUILD)/,$(APP_SRC:.c=.o) $(HAL_SRC:.c=.o) $(ASM_SRC:.s=.o))

# Our own code is held to a stricter warning level than vendor HAL code.
$(BUILD)/Core/Src/%.o: CFLAGS += -Wextra -Werror

.PHONY: all clean test sim
all: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
	@$(SIZE) $(BUILD)/$(TARGET).elf

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(MCU) -x assembler-with-cpp -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary $< $@

CORE_SRC := Core/Src/la_trigger.c Core/Src/la_capture.c Core/Src/sump.c

test: sim
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -ICore/Inc tests/test_main.c $(CORE_SRC) -o $(BUILD)/host_tests
	$(BUILD)/host_tests

# Host simulator: the firmware's protocol and capture code on stdin/stdout.
sim:
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ICore/Inc tests/sim_device.c $(CORE_SRC) -o $(BUILD)/sim_device

clean:
	rm -rf $(BUILD)
