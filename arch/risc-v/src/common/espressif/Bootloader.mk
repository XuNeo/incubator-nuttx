############################################################################
# arch/risc-v/src/common/espressif/Bootloader.mk
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

# Remove quotes from CONFIG_ESPRESSIF_CHIP_SERIES configuration

CHIP_SERIES = $(patsubst "%",%,$(CONFIG_ESPRESSIF_CHIP_SERIES))

TOOLSDIR             = $(TOPDIR)/tools/espressif
CHIPDIR              = $(TOPDIR)/arch/risc-v/src/chip
BOOTLOADER_SRCDIR    = $(CHIPDIR)/bootloader
BOOTLOADER_OUTDIR    = $(BOOTLOADER_SRCDIR)/out
BOOTLOADER_CONFIG    = $(BOOTLOADER_SRCDIR)/bootloader.conf

# MCUboot

MCUBOOT_SRCDIR     = $(BOOTLOADER_SRCDIR)/mcuboot
MCUBOOT_ESPDIR     = $(MCUBOOT_SRCDIR)/boot/espressif
MCUBOOT_TOOLCHAIN  = $(TOOLSDIR)/mcuboot_toolchain_espressif.cmake
HALDIR             = $(BOOTLOADER_SRCDIR)/esp-hal-3rdparty-mcuboot

ifndef MCUBOOT_VERSION
	MCUBOOT_VERSION = $(CONFIG_ESPRESSIF_MCUBOOT_VERSION)
endif

# MCUboot tarball configuration
MCUBOOT_TARBALL = $(MCUBOOT_VERSION).tar.gz
MCUBOOT_UNPACKNAME = mcuboot-$(patsubst v%,%,$(MCUBOOT_VERSION))
MCUBOOT_URL_BASE = https://github.com/mcu-tools/mcuboot/archive/refs/tags

# mbedtls submodule for MCUboot (queried from MCUboot repository)
# For MCUboot v2.1.0, mbedtls is at commit: 1a0b220d9e25a9fdedf750bfb86664cf6e032b7c
MCUBOOT_MBEDTLS_VERSION = 1a0b220d9e25a9fdedf750bfb86664cf6e032b7c
MCUBOOT_MBEDTLS_TARBALL = $(MCUBOOT_MBEDTLS_VERSION).tar.gz
MCUBOOT_MBEDTLS_URL_BASE = https://github.com/Mbed-TLS/mbedtls/archive

ifndef ESP_HAL_3RDPARTY_VERSION_FOR_MCUBOOT
	ESP_HAL_3RDPARTY_VERSION_FOR_MCUBOOT = 911dbec8e4a92e70056b58a3d2b0d965b8b7bcc9
endif

# HAL tarball configuration for MCUboot
ESP_HAL_MCUBOOT_TARBALL = $(ESP_HAL_3RDPARTY_VERSION_FOR_MCUBOOT).tar.gz
ESP_HAL_MCUBOOT_UNPACKNAME = esp-hal-3rdparty-$(ESP_HAL_3RDPARTY_VERSION_FOR_MCUBOOT)
ESP_HAL_MCUBOOT_URL_BASE = https://github.com/espressif/esp-hal-3rdparty/archive

# Helpers for creating the configuration file

cfg_en  = echo "$(1)=$(if $(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT),1,y)";
cfg_dis = echo "$(1)=$(if $(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT),0,n)";
cfg_val = echo "$(1)=$(2)";

$(BOOTLOADER_SRCDIR):
	$(Q) mkdir -p $(BOOTLOADER_SRCDIR) &>/dev/null

$(BOOTLOADER_CONFIG): $(TOPDIR)/.config $(BOOTLOADER_SRCDIR)
	$(Q) echo "Creating Bootloader configuration"
	$(Q) { \
		$(call cfg_en,NON_OS_BUILD) \
		$(if $(CONFIG_ESPRESSIF_FLASH_2M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHSIZE_2MB)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_4M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHSIZE_4MB)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_MODE_DIO),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHMODE_DIO)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_MODE_DOUT),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHMODE_DOUT)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_MODE_QIO),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHMODE_QIO)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_MODE_QOUT),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHMODE_QOUT)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_FREQ_80M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHFREQ_80M)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_FREQ_64M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHFREQ_64M)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_FREQ_48M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHFREQ_48M)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_FREQ_40M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHFREQ_40M)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_FREQ_26M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHFREQ_26M)) \
		$(if $(CONFIG_ESPRESSIF_FLASH_FREQ_20M),$(call cfg_en,CONFIG_ESPTOOLPY_FLASHFREQ_20M)) \
		$(call cfg_val,CONFIG_ESPTOOLPY_FLASHFREQ,$(CONFIG_ESPRESSIF_FLASH_FREQ)) \
	} > $(BOOTLOADER_CONFIG)
ifeq ($(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT),y)
	$(Q) { \
		$(call cfg_en,CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) \
		$(call cfg_val,CONFIG_ESP_BOOTLOADER_OFFSET,0x0000) \
		$(call cfg_val,CONFIG_ESP_BOOTLOADER_SIZE,0xF000) \
		$(call cfg_val,CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS,$(CONFIG_ESPRESSIF_OTA_PRIMARY_SLOT_OFFSET)) \
		$(call cfg_val,CONFIG_ESP_APPLICATION_SIZE,$(CONFIG_ESPRESSIF_OTA_SLOT_SIZE)) \
		$(call cfg_val,CONFIG_ESP_IMAGE0_SECONDARY_START_ADDRESS,$(CONFIG_ESPRESSIF_OTA_SECONDARY_SLOT_OFFSET)) \
		$(call cfg_en,CONFIG_ESP_MCUBOOT_WDT_ENABLE) \
		$(call cfg_en,CONFIG_LIBC_NEWLIB) \
		$(call cfg_val,CONFIG_ESP_SCRATCH_OFFSET,$(CONFIG_ESPRESSIF_OTA_SCRATCH_OFFSET)) \
		$(call cfg_val,CONFIG_ESP_SCRATCH_SIZE,$(CONFIG_ESPRESSIF_OTA_SCRATCH_SIZE)) \
		$(call cfg_en,CONFIG_ESP_CONSOLE_UART) \
		$(if $(CONFIG_UART0_SERIAL_CONSOLE),$(call cfg_val,CONFIG_ESP_CONSOLE_UART_NUM,0)) \
		$(if $(CONFIG_UART1_SERIAL_CONSOLE),$(call cfg_val,CONFIG_ESP_CONSOLE_UART_NUM,1)) \
		$(if $(CONFIG_UART0_SERIAL_CONSOLE),$(call cfg_val,CONFIG_ESP_CONSOLE_ROM_SERIAL_PORT_NUM,0)) \
		$(if $(CONFIG_UART1_SERIAL_CONSOLE),$(call cfg_val,CONFIG_ESP_CONSOLE_ROM_SERIAL_PORT_NUM,1)) \
		$(if $(CONFIG_ESPRESSIF_USBSERIAL),$(call cfg_val,CONFIG_ESP_CONSOLE_UART_NUM,0)) \
		$(if $(CONFIG_ESPRESSIF_SECURE_FLASH_ENC_ENABLED),$(call cfg_en,CONFIG_SECURE_FLASH_ENC_ENABLED)) \
		$(if $(CONFIG_ESPRESSIF_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT),$(call cfg_en,CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT)) \
		$(if $(CONFIG_ESPRESSIF_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC),$(call cfg_en,CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC)) \
		$(if $(CONFIG_ESPRESSIF_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC),$(call cfg_en,CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC)) \
		$(if $(CONFIG_ESPRESSIF_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE),$(call cfg_en,CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE)) \
		$(call cfg_val,CONFIG_BOOTLOADER_LOG_LEVEL,3) \
	} >> $(BOOTLOADER_CONFIG)
ifeq ($(CONFIG_ESPRESSIF_EFUSE_VIRTUAL_KEEP_IN_FLASH),y)
	$(Q) { \
		$(call cfg_en,CONFIG_EFUSE_VIRTUAL) \
		$(call cfg_en,CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH) \
		$(call cfg_val,CONFIG_EFUSE_VIRTUAL_OFFSET,$(CONFIG_ESPRESSIF_EFUSE_VIRTUAL_KEEP_IN_FLASH_OFFSET)) \
		$(call cfg_val,CONFIG_EFUSE_VIRTUAL_SIZE,$(CONFIG_ESPRESSIF_EFUSE_VIRTUAL_KEEP_IN_FLASH_SIZE)) \
	} >> $(BOOTLOADER_CONFIG)
endif
endif

ifeq ($(CONFIG_ESPRESSIF_SIMPLE_BOOT),y)
bootloader:
	$(Q) echo "Using direct bootloader to boot NuttX."
else
ifeq ($(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT),y)

BOOTLOADER_BIN = $(TOPDIR)/mcuboot-$(CHIP_SERIES).bin

# Download MCUboot tarball
$(MCUBOOT_TARBALL):
	$(call DOWNLOAD,$(MCUBOOT_URL_BASE),$(MCUBOOT_TARBALL))

# Download mbedtls for MCUboot
$(MCUBOOT_MBEDTLS_TARBALL):
	$(call DOWNLOAD,$(MCUBOOT_MBEDTLS_URL_BASE),$(MCUBOOT_MBEDTLS_TARBALL))

# Unpack MCUboot and mbedtls submodule
$(MCUBOOT_SRCDIR): $(BOOTLOADER_SRCDIR) $(MCUBOOT_TARBALL) $(MCUBOOT_MBEDTLS_TARBALL)
	$(Q) echo "Unpacking: MCUboot $(MCUBOOT_VERSION)"
	$(Q) tar xzf $(MCUBOOT_TARBALL) -C $(BOOTLOADER_SRCDIR)
	$(Q) mv $(BOOTLOADER_SRCDIR)/$(MCUBOOT_UNPACKNAME) $(MCUBOOT_SRCDIR)
	$(Q) echo "Unpacking: mbedtls for MCUboot"
	$(Q) mkdir -p $(MCUBOOT_SRCDIR)/ext/mbedtls
	$(Q) tar xzf $(MCUBOOT_MBEDTLS_TARBALL) --strip-components=1 -C $(MCUBOOT_SRCDIR)/ext/mbedtls
	$(Q) touch $(MCUBOOT_SRCDIR)

# Download HAL tarball for MCUboot
$(ESP_HAL_MCUBOOT_TARBALL):
	$(call DOWNLOAD,$(ESP_HAL_MCUBOOT_URL_BASE),$(ESP_HAL_MCUBOOT_TARBALL))

# Unpack HAL for MCUboot
$(HALDIR): $(ESP_HAL_MCUBOOT_TARBALL)
	$(Q) echo "Unpacking: ESP HAL 3rdparty for MCUboot $(ESP_HAL_3RDPARTY_VERSION_FOR_MCUBOOT)"
	$(Q) tar xzf $(ESP_HAL_MCUBOOT_TARBALL)
	$(Q) mv $(ESP_HAL_MCUBOOT_UNPACKNAME) $(HALDIR)
	$(Q) touch $(HALDIR)

$(BOOTLOADER_BIN): $(HALDIR) $(MCUBOOT_SRCDIR) $(BOOTLOADER_CONFIG)
	$(Q) echo "Building MCUboot"
	$(Q) $(TOOLSDIR)/build_mcuboot.sh \
		-c $(CHIP_SERIES) \
		-f $(BOOTLOADER_CONFIG) \
		-p $(BOOTLOADER_SRCDIR) \
		-e $(HALDIR) \
		-d $(MCUBOOT_TOOLCHAIN)
	$(call COPYFILE, $(BOOTLOADER_OUTDIR)/mcuboot-$(CHIP_SERIES).bin, $(TOPDIR))

bootloader: $(BOOTLOADER_CONFIG) $(BOOTLOADER_BIN)

clean_bootloader:
	$(call DELDIR,$(HALDIR))
	$(call DELDIR,$(BOOTLOADER_SRCDIR))
	$(call DELFILE,$(BOOTLOADER_BIN))
	$(call DELFILE,$(MCUBOOT_TARBALL))
	$(call DELFILE,$(MCUBOOT_MBEDTLS_TARBALL))
	$(call DELFILE,$(ESP_HAL_MCUBOOT_TARBALL))
endif
endif
