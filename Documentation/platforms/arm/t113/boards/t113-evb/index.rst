=========
t113-evb
=========

.. tags:: chip:t113, chip:allwinner, arch:cortex-a7

T113-S3 evaluation board with dual Cortex-A7, 128MB DDR3, SPI NAND flash,
USB OTG, UART, I2C, and JTAG debug interface.

Board Features
==============

- **SoC**: Allwinner T113-S3 (dual Cortex-A7 @ 1.2GHz)
- **Memory**: 128MB DDR3 (integrated in SoC package)
- **Storage**: SPI NAND flash
- **Debug**: JTAG (20-pin) + UART0 serial console (115200 baud)
- **USB**: USB OTG with High-Speed 480Mbps support
- **Peripherals**: GPIO (banks B-G), RTC, PWM (8ch), GPADC (4ch), I2C, SPI

Pin Connections
===============

UART0 (serial console):

  ====== =====
  UART0  PINS
  ====== =====
  TX     PE2
  RX     PE3
  ====== =====

USB OTG:

  ========= ============
  USB       Address
  ========= ============
  OTG Base  0x04100000
  PHY Base  0x04100400
  ========= ============

SPI0 (NAND flash):

  ====== =====
  SPI0   PINS
  ====== =====
  CLK    PC2
  MOSI   PC4
  MISO   PC3
  CS     PC7
  ====== =====

I2C (TWI0):

  ====== =====
  TWI0   PINS
  ====== =====
  SCL    PE12
  SDA    PE13
  ====== =====

Toolchain
=========

Any ARM cross-compiler that supports Cortex-A7 can be used.  The
recommended toolchain is ``arm-none-eabi-gcc`` from ARM::

    $ arm-none-eabi-gcc --version
    arm-none-eabi-gcc (GNU Arm Embedded Toolchain) 13.x

Building
========

Use CMake with Ninja::

    $ cd nuttx
    $ cmake -B build -GNinja -DBOARD_CONFIG=t113-evb:<config> -DNUTTX_APPS_DIR=../apps
    $ ninja -C build

Where ``<config>`` is one of the configurations listed below.

Loading Firmware
================

The T113-EVB uses xfel + JLink for DDR-based development:

1. Enter FEL mode (power on with no valid NAND boot image, or WDT reset)
2. Initialize DDR and write firmware::

    $ xfel ddr t113-s3
    $ xfel write 0x40000000 build/nuttx.bin

3. Start execution via JLink::

    $ JLinkExe -device R528S3-Core0 -if JTAG -speed 20000
    J-Link> halt
    J-Link> SetPC 0x40000040
    J-Link> go

Serial Console
==============

The default serial console is on UART0 at 115200 baud.  Connect
to ``/dev/ttyUSB0`` (or equivalent) on the host::

    $ minicom -D /dev/ttyUSB0 -b 115200

Configuration Directories
=========================

nsh
---

Basic NuttShell on UART0, single-core, with ostest enabled.
Includes full peripheral support (GPIO, RTC, PWM, ADC, WDT).

::

    nsh> hello
    Hello, World!!
    nsh> ostest
    ...
    ostest_main: Exiting with status 0

nsh_smp
-------

Same as ``nsh`` but with dual-core SMP enabled (``CONFIG_SMP_NCPUS=2``).
Includes ostest with SMP call test.

::

    nsh> ostest
    ...
    smp_call_test: Test success
    ostest_main: Exiting with status 0

adb
---

NuttShell on UART0 with USB ADB (Android Debug Bridge) support at
High-Speed 480Mbps.  Run ``adbd &`` to start the ADB daemon, then
connect from the host::

    nsh> adbd &
    $ adb devices
    List of devices attached
    1234    device
    $ adb shell hello
    Hello, World!!

usbnsh
------

NuttShell with USB CDC-ACM as the console device (``/dev/console``).
The serial console is on USB instead of UART0.  After boot, a
``/dev/ttyACMx`` device appears on the host.

**Status**: USB CDC-ACM enumerates at 480Mbps.  NSH console binding
is work-in-progress.

composite
---------

NuttShell on UART0 with USB composite device: CDC-ACM + ADB at
High-Speed 480Mbps.  Use ``conn`` to start the composite USB device::

    nsh> conn
    $ lsusb -t | grep 480M
    |__ Port 1: Dev XX, If 0, Class=Communications, Driver=cdc_acm, 480M
    |__ Port 1: Dev XX, If 2, Class=Vendor Specific Class, Driver=, 480M

CDC-ACM data can be sent/received via ``/dev/ttyACMx`` on the host.
ADB is accessible after running ``adbd &`` on the device.

spinand
-------

Minimal NuttShell on UART0 with SPI DMA + NAND MTD + LittleFS.
No USB, no additional peripherals.  For SPI NAND flash testing.

bootloader
----------

Boot0 configuration for NAND cold-start.  Includes DDR initialization,
PLL setup, and SPI NAND image loader.  Requires ``mksunxi.py`` to
patch the eGON header checksum::

    $ python3 tools/mksunxi.py build/nuttx.bin

**Status**: Compiles successfully.  NAND deployment not yet tested.

Peripheral Support
==================

  ================ ========== ==============
  Peripheral       Status     Driver
  ================ ========== ==============
  USB CDC-ACM      Working    t113_usbdev.c
  USB ADB          Working    t113_usbdev.c
  USB Composite    Working    t113_composite.c
  USB High-Speed   Working    480Mbps
  GPIO (banks B-G) Working    t113_gpio.c
  RTC              Working    t113_rtc.c
  PWM (8ch)        Working    t113_pwm.c
  GPADC (4ch)      Working    t113_adc.c
  Watchdog         Working    t113_wdt.c
  Reset Cause      Working    t113_systemreset.c
  CPU Idle (WFI)   Working    t113_idle.c
  Tickless         Working    arm_timer.c
  SMP (dual-core)  Working    2x Cortex-A7
  SPI NAND MTD     Working    t113_spi.c
  I2C (TWI0)       Working    t113_twi.c
  DMA              Working    t113_dma.c
  ================ ========== ==============

Test Suite
==========

The ``t113_test`` application runs 11 hardware verification tests::

    nsh> t113_test
    === T113 Integration Test Suite ===
    [PASS] test_mtd
    [PASS] test_littlefs
    [PASS] test_lfs_multi
    [PASS] test_timer (elapsed 210ms)
    [PASS] test_i2c
    [PASS] test_heap (arena=127MB)
    [PASS] test_rtc
    [PASS] test_gpio (PB0 output toggle verified)
    [PASS] test_pwm (1kHz 50%, cnt running)
    [PASS] test_adc (ch0 ~1791mV)
    [PASS] test_smp (cpu0=0 cpu1=2000)
    === T113 Test Complete: ALL PASS (0 failures) ===
