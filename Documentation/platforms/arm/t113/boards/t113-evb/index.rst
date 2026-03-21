=========
t113-evb
=========

.. tags:: chip:t113, chip:allwinner, arch:cortex-a7

The T113-EVB is an evaluation board based on the Allwinner T113-S3 SoC
(dual Cortex-A7 @ 1.2GHz, 128MB DDR3, SPI NAND flash, USB OTG).

Board Features
==============

- Allwinner T113-S3 (dual Cortex-A7 + HiFi4 DSP)
- 128MB DDR3 (integrated in SoC package)
- SPI NAND flash
- USB 2.0 OTG (High-Speed 480Mbps)
- UART, I2C, SPI, PWM, GPADC, GPIO
- JTAG debug interface

Serial Console
==============

The default serial console is UART0 (PE2/PE3) at 115200 baud.

Configurations
==============

nsh
---

Basic NuttShell on UART0, single-core.  Includes ostest.

nsh_smp
-------

Same as ``nsh`` with dual-core SMP enabled.  Includes ostest.

adb
---

NuttShell on UART0 with USB ADB at 480Mbps.  Run ``adbd &`` then
connect from host::

    $ adb shell hello
    Hello, World!!

usbnsh
------

NuttShell over USB CDC-ACM serial console at 480Mbps.
After boot, connect to ``/dev/ttyACMx`` on the host.
You may need to press ENTER a few times before NSH shows up.

composite
---------

NuttShell on UART0 with USB CDC-ACM + ADB composite device at
480Mbps.  Use ``conn`` to start the composite USB device.

spinand
-------

Minimal NuttShell on UART0 with SPI NAND + LittleFS.  No USB.

bootloader
----------

Boot0 cold-start from NAND.  Includes DDR init and SPI NAND loader.
Requires ``tools/mksunxi.py`` to patch eGON header checksum.

Peripheral Support
==================

  ================ ==========
  Peripheral       Status
  ================ ==========
  USB Device (HS)  Working
  SMP (dual-core)  Working
  SPI NAND + DMA   Working
  I2C              Working
  GPIO             Working
  RTC              Working
  PWM              Working
  GPADC            Working
  Watchdog         Working
  Tickless Timer   Working
  ================ ==========
