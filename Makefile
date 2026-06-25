##############################################################################
# Makefile - STM32-Flight-Controller-FreeRTOS
# Target: STM32F407VG (STM32F407G-DISC1 Discovery board)
# Toolchain: arm-none-eabi-gcc
##############################################################################

TARGET   = flight_controller
BUILDDIR = build

# ---- Toolchain ----
PREFIX   = arm-none-eabi-
CC       = $(PREFIX)gcc
AS       = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY  = $(PREFIX)objcopy
SIZE     = $(PREFIX)size

# ---- MCU / FPU ----
MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# ---- Defines ----
DEFS = -DSTM32F407xx -DHSE_VALUE=8000000U

# ---- Include paths ----
INCLUDES = \
  -ICore/Inc \
  -IDrivers/CMSIS/Include \
  -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
  -IThird_Party/FreeRTOS/include \
  -IThird_Party/FreeRTOS/portable/GCC/ARM_CM4F

# ---- Sources ----
C_SOURCES = \
  Core/Src/main.c \
  Core/Src/delay.c \
  Core/Src/ssd1306.c \
  Core/Src/stm32f4xx_it.c \
  Core/Src/syscalls.c \
  Core/Src/sysmem.c \
  Core/Src/system_stm32f4xx.c \
  Core/Src/flight_core.c \
  Core/Src/board/board_init.c \
  Core/Src/board/system_clock.c \
  Core/Src/drivers/bmp280.c \
  Core/Src/drivers/i2c_bus.c \
  Core/Src/drivers/mpu6050.c \
  Third_Party/FreeRTOS/croutine.c \
  Third_Party/FreeRTOS/event_groups.c \
  Third_Party/FreeRTOS/list.c \
  Third_Party/FreeRTOS/queue.c \
  Third_Party/FreeRTOS/stream_buffer.c \
  Third_Party/FreeRTOS/tasks.c \
  Third_Party/FreeRTOS/timers.c \
  Third_Party/FreeRTOS/portable/GCC/ARM_CM4F/port.c \
  Third_Party/FreeRTOS/portable/MemMang/heap_4.c

ASM_SOURCES = Core/Startup/startup_stm32f407xx.s

LDSCRIPT = STM32F407VGTX_FLASH.ld

# ---- Flags ----
CFLAGS  = $(MCU_FLAGS) $(DEFS) $(INCLUDES) -O2 -g3 -std=gnu11 -Wall \
          -ffunction-sections -fdata-sections -fno-strict-aliasing
ASFLAGS = $(MCU_FLAGS) -g3
LDFLAGS = $(MCU_FLAGS) -specs=nano.specs -u _printf_float \
          -T$(LDSCRIPT) \
          -Wl,-Map=$(BUILDDIR)/$(TARGET).map \
          -Wl,--gc-sections \
          -lc -lm -lnosys

OBJECTS = $(addprefix $(BUILDDIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILDDIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

.PHONY: all clean flash

all: $(BUILDDIR)/$(TARGET).elf $(BUILDDIR)/$(TARGET).hex $(BUILDDIR)/$(TARGET).bin
	$(SIZE) $(BUILDDIR)/$(TARGET).elf

$(BUILDDIR)/%.o: %.c Makefile | $(BUILDDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.s Makefile | $(BUILDDIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILDDIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILDDIR)/%.hex: $(BUILDDIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILDDIR)/%.bin: $(BUILDDIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)


flash: $(BUILDDIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg \
	        -f target/stm32f4x.cfg \
	        -c "program $< verify reset exit"