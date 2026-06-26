# STM32-Flight-Controller-FreeRTOS — Ported to STM32F407G-DISC1

This is the original `atessahin/STM32-Flight-Controller-FreeRTOS` firmware, ported and made buildable
for your hardware (STM32F407G-DISC1, Windows, onboard ST-Link). All sensor drivers, the FreeRTOS task
structure, and the complementary-filter logic are untouched — only the device target and build system
were added/fixed.

## What was actually wrong / missing

The repository on GitHub contains complete application source but **no build system at all** — no
Makefile, no STM32CubeIDE project, nothing. It also targeted a different MCU (STM32F411VE) than what
you have (STM32F407VG). Specifically, I made these changes:

1. **Added a build system.** `Makefile` (for `make`) and `build.bat` (zero-dependency, no `make` needed)
   — both do a full one-shot compile + link with `arm-none-eabi-gcc`.
2. **Retargeted from STM32F411VE → STM32F407VG**: added the official ST CMSIS device header
   (`stm32f407xx.h`), swapped in ST's official `startup_stm32f407xx.s`, and resized the linker script
   to the F407's 1MB flash / 128KB RAM (`STM32F407VGTX_FLASH.ld`). RAM size happened to already match.
3. **Patched the new startup file's vector table** so `SVCall`/`PendSV` point directly at FreeRTOS's
   `vPortSVCHandler`/`xPortPendSVHandler` (this exact pattern was already done in the original repo's
   F411 startup file — `FreeRTOSConfig.h` only renames the SysTick handler, not these two, so the
   vector table has to route them directly or FreeRTOS's scheduler will never start).
4. **Fixed a latent clock bug**: nothing in the original repo ever defined `HSE_VALUE`, so it silently
   defaulted to 25 MHz inside `system_stm32f4xx.c`, even though both this board and the F411 target use
   an 8 MHz crystal. This doesn't affect the PLL itself (clocks are set via direct register writes and
   would still run at 84 MHz), but it corrupts the `SystemCoreClock` variable, which FreeRTOS uses to
   compute the SysTick reload value — i.e. your "250 Hz" loop and `delay_us()` would silently run at the
   wrong real-world rate. Fixed by passing `-DHSE_VALUE=8000000U` at compile time.
5. **Fixed a font out-of-bounds bug**: the OLED font table only has glyphs for chars 32–90 (space
   through `Z`), but the altitude line printed a lowercase `m` (char 109) — past the end of the array.
   Changed it to `M`.

I compiled this myself end-to-end with `arm-none-eabi-gcc` and verified the linked ELF (entry point,
vector table contents, and that `vPortSVCHandler`/`xPortPendSVHandler`/`SysTick_Handler` all resolved to
real code, not the default fault handler) before handing it to you. It builds clean — 26.6KB flash /
~78.5KB RAM used, plenty of headroom on this chip.

## 1. Hardware wiring

Your F407G-DISC1 already uses I2C1 (PB6/PB7) on-board for its accelerometer and audio codec — that's
fine, you're just sharing the same bus. Wire your three external modules to the GPIO header like this:

| Module signal | Connect to (Discovery header) |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | PB6 |
| SDA | PB7 |

All three modules (MPU6050, BME280, SSD1306) share the same SCL/SDA lines — this is a single I2C bus
with three devices at three different addresses (0x68, 0x76, 0x3C). No address conflicts.

If you get intermittent I2C timeouts once everything is wired (longer wires = weaker signal), add a
4.7kΩ pull-up resistor from SDA to 3V3 and another from SCL to 3V3 — the firmware enables the MCU's
internal weak pull-ups but breadboard wiring sometimes needs the extra help.

**Two things worth checking on your exact modules before assuming a wiring problem:**
- BME280 breakout boards default to I2C address **0x76** if the SDO pin is grounded, or **0x77** if it's
  pulled to VCC. The firmware is hardcoded to 0x76 (`Core/Inc/bmp280.h`). If your altitude reading never
  updates, this is the first thing to check.
- SSD1306 modules are almost always 0x3C, but a few clones use 0x3D (`Core/Inc/ssd1306.h`).

## 2. Install the toolchain (Windows)

You need two things, both free, official, single installers:

1. **ARM GNU Toolchain** (compiler): https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
   — download the Windows installer (`arm-gnu-toolchain-...-mingw-w64-x86_64-arm-none-eabi.exe`), run it,
   and tick **"Add path to environment variable"** when offered.
2. **STM32CubeProgrammer** (flashing tool): https://www.st.com/en/development-tools/stm32cubeprog.html
   — free, requires an ST account to download. Install with defaults.

You do **not** need `make` — `build.bat` calls the compiler directly.

You also do **not** need a separate ST-Link or DFU cable: the F407G-DISC1 has an **ST-Link/V2 built
into the board itself**. Just plug your USB cable into the Mini-USB port labeled for ST-Link power (the
one you'd normally use anyway) — it's both your power supply and your programmer. Forget the DFU/BOOT0
route entirely; it's unnecessary extra steps for hardware that already has a debugger on it.

After installing, open a new Command Prompt and confirm both are on PATH:
```
arm-none-eabi-gcc --version
STM32_Programmer_CLI --version
```
If either command isn't found, restart the Command Prompt (PATH changes need a fresh shell) or re-run
the installer and check the PATH option.

## 3. Build

Unzip the project, open Command Prompt in that folder, and run:
```
build.bat
```
You should see `BUILD OK -> build\flight_controller.bin`. A prebuilt copy of the same binary is already
included in `build\` in case you want to flash immediately and build later.

## 4. Flash

With the board connected via USB (ST-Link port):
```
flash.bat
```
This calls `STM32_Programmer_CLI -c port=SWD -w build\flight_controller.bin 0x08000000 -v -rst`, which
connects over SWD via the onboard ST-Link, writes the binary at the start of flash, verifies it, and
resets the chip to run it.

If you'd rather use a GUI: open **STM32CubeProgrammer**, click **Connect** (ST-LINK / SWD, defaults are
fine), then **Open file** → select `build\flight_controller.bin`, set the start address to `0x08000000`,
and click **Download**.

## 5. Verify it's working

Power-up sequence on the OLED should be:
1. `INITIALIZING... / DO NOT MOVE!` (~1s)
2. `CALIBRATING...` (~1s, gyro zero-offset calibration — keep the board still)
3. `SYSTEM READY! / READY FOR TAKEOFF!` (~1s)
4. Live `P:`, `R:`, `A:` (pitch, roll, altitude) updating a few times a second

Tilt the board — pitch/roll should respond within a fraction of a second and settle back near 0 when
level. Altitude should be near 0.0 right after boot (it calibrates to ground-level pressure at startup)
and should tick up if you raise the board significantly (a few tens of cm won't move it much — the
BME280's resolution is roughly ±0.5–1m of barometric noise, that's expected, not a bug).

## Troubleshooting

**OLED stays blank, nothing happens at all.**
Check VCC/GND wiring first — most "nothing happens" cases are power, not firmware. If wiring is good,
connect a debugger (STM32CubeProgrammer's built-in ST-Link GDB server, or just re-flash and watch for
errors) — a hang here usually means an I2C device isn't responding and the bare-metal I2C driver's
timeout loop is eating cycles before `i2c_software_reset()` kicks in, which can take a moment on first
boot if a module is miswired.

**OLED shows "INITIALIZING" then freezes forever, never reaches "CALIBRATING".**
`mpu6050_init()`/calibration is blocking on I2C reads. Means the MPU6050 isn't responding — check its
address (0x68 — verify the AD0 pin isn't pulled high, which would shift it to 0x69) and wiring.

**Display works, pitch/roll update, but altitude is always 0.0 or frozen.**
BME280 not responding — almost always the 0x76 vs 0x77 address issue mentioned above. Check your
module's SDO pin strapping, or just try editing `BMP280_ADDR` in `Core/Inc/bmp280.h` to `0x77` and
rebuild.

**`STM32_Programmer_CLI` can't find/connect to the board.**
- Check Device Manager for the ST-Link under USB devices; if it shows up with a yellow warning icon,
  install the ST-Link USB driver (bundled with STM32CubeProgrammer, or get it standalone from ST).
- Make sure you're using the ST-Link USB port, not the USB OTG FS port (the board has two USB
  connectors — they are not interchangeable).
- Try a different USB cable/port; ST-Link clones are sometimes picky about cable quality.

**Build fails with "arm-none-eabi-gcc is not recognized".**
PATH wasn't updated, or you're using a Command Prompt opened before the toolchain install completed.
Open a brand new Command Prompt window and retry; if it still fails, manually add the toolchain's `bin`
folder (something like `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\<version>\bin`) to PATH.

**Pitch/roll readings drift slowly over time / never quite settle.**
This is inherent to a pure complementary filter with no magnetometer/yaw reference — it's a known
characteristic of the algorithm, not a bug introduced by this port. Out of scope for "get it working,"
but worth knowing if you plan to fly on this attitude estimate.

**You want more performance headroom.**
This firmware currently runs the F407 at a conservative 84 MHz (the same clock target as the original
F411 firmware). The F407 can run up to 168 MHz. I left this unchanged deliberately — it works, and
bumping the clock means re-deriving the PLL/flash-wait-state/peripheral-prescaler values from scratch,
which is exactly the kind of optimization that should come after you've confirmed the system flies, not
before.
