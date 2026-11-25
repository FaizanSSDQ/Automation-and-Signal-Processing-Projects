# Standalone build for the NUCLEO-L152RE FSK modem.
# Works with any arm-none-eabi-gcc (STM32CubeIDE bundles one; CI uses the Ubuntu package).
#
#   make            build build/fsk_modem.elf/.hex/.bin
#   make test       run host unit tests for the portable modules
#   make clean
#
# Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

TARGET   := fsk_modem
BUILD    := build
PREFIX   ?= arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
SIZE     := $(PREFIX)size
HOST_CC  ?= cc

MCU      := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFS     := -DUSE_HAL_DRIVER -DSTM32L152xE
INCS     := -ICore/Inc \
            -IDrivers/STM32L1xx_HAL_Driver/Inc \
            -IDrivers/STM32L1xx_HAL_Driver/Inc/Legacy \
            -IDrivers/CMSIS/Device/ST/STM32L1xx/Include \
            -IDrivers/CMSIS/Include

CFLAGS   := $(MCU) $(DEFS) $(INCS) -std=gnu11 -Os -g3 -Wall -ffunction-sections -fdata-sections
LDFLAGS  := $(MCU) -TSTM32L152RETX_FLASH.ld --specs=nano.specs --specs=nosys.specs \
            -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

APP_SRC  := $(wildcard Core/Src/*.c)
HAL_SRC  := $(wildcard Drivers/STM32L1xx_HAL_Driver/Src/*.c)
ASM_SRC  := Core/Startup/startup_stm32l152retx.s
OBJS     := $(addprefix $(BUILD)/,$(APP_SRC:.c=.o) $(HAL_SRC:.c=.o) $(ASM_SRC:.s=.o))

# Our own code is held to a stricter warning level than vendor HAL code.
$(BUILD)/Core/Src/%.o: CFLAGS += -Wextra -Werror

.PHONY: all clean test
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

test:
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -ICore/Inc \
	    tests/test_main.c Core/Src/fsk_mod.c Core/Src/fsk_demod.c Core/Src/fsk_tables.c -lm -o $(BUILD)/host_tests
	$(BUILD)/host_tests

clean:
	rm -rf $(BUILD)
