# Project 7: FreeRTOS on STM32F407
# Toolchain: arm-none-eabi-gcc

TARGET  = freertos_project
MCU     = cortex-m4
FPU     = fpv4-sp-d16
FLOAT   = hard

# Toolchain
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Directories
SRC_DIR     = src
FREERTOS_DIR = FreeRTOS/Source

# Source files — project
SRCS = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/sensor_task.c \
    $(SRC_DIR)/processing_task.c \
    $(SRC_DIR)/display_task.c \
    $(SRC_DIR)/priority_inversion_demo.c \
    $(SRC_DIR)/ssd1306.c \
    $(SRC_DIR)/uart.c \
    $(SRC_DIR)/i2c.c \
    $(SRC_DIR)/mpu6050.c

# Source files — FreeRTOS kernel
SRCS += \
    $(FREERTOS_DIR)/tasks.c \
    $(FREERTOS_DIR)/queue.c \
    $(FREERTOS_DIR)/list.c \
    $(FREERTOS_DIR)/timers.c \
    $(FREERTOS_DIR)/event_groups.c \
    $(FREERTOS_DIR)/stream_buffer.c \
    $(FREERTOS_DIR)/portable/GCC/ARM_CM4F/port.c \
    $(FREERTOS_DIR)/portable/MemMang/heap_4.c

# Include paths
INCS = \
    -I. \
    -I$(SRC_DIR) \
    -I$(FREERTOS_DIR)/include \
    -I$(FREERTOS_DIR)/portable/GCC/ARM_CM4F \
    -ICMSIS/Include \
    -ICMSIS/Device/ST/STM32F4xx/Include

# Compiler flags
CFLAGS  = -mcpu=$(MCU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT)
CFLAGS += -O2 -g3
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F407xx
CFLAGS += $(INCS)

# Linker flags
LDFLAGS  = -mcpu=$(MCU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT)
LDFLAGS += -T STM32F407VGTx_FLASH.ld
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(TARGET).map
LDFLAGS += -lm   # math library for sqrtf
LDFLAGS += --specs=nano.specs  # use nano libc (smaller printf)
LDFLAGS += --specs=nosys.specs

# Object files
OBJS = $(SRCS:.c=.o)

# Build rules
all: $(TARGET).elf $(TARGET).bin size

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(TARGET).elf
	$(SIZE) $

flash: $(TARGET).bin
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	        -c "program $(TARGET).bin 0x08000000 verify reset exit"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).map

.PHONY: all flash clean size