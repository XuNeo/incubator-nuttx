/****************************************************************************
 * arch/arm/src/t113/hardware/t113_usb.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_T113_HARDWARE_T113_USB_H
#define __ARCH_ARM_SRC_T113_HARDWARE_T113_USB_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_USB_OTG_BASE       0x04100000
#define T113_USB_PHY_BASE       (T113_USB_OTG_BASE + 0x0400)

/* CCU USB registers */

#define T113_CCU_BASE           0x02001000
#define T113_CCU_USB0_CLK       (T113_CCU_BASE + 0x0a70)
#define T113_CCU_USB_BGR        (T113_CCU_BASE + 0x0a8c)

/* USB0_CLK_REG bits */

#define USB0_CLK_OHCI_EN        (1 << 31)
#define USB0_CLK_PHYRST_DEASSERT (1 << 30)

/* USB_BGR_REG bits */

#define USB_BGR_OTG0_RST        (1 << 24)
#define USB_BGR_EHCI0_RST       (1 << 20)
#define USB_BGR_OHCI0_RST       (1 << 16)
#define USB_BGR_OTG0_GATING     (1 << 8)
#define USB_BGR_EHCI0_GATING    (1 << 4)
#define USB_BGR_OHCI0_GATING    (1 << 0)

/* MUSB register offsets from USB_OTG_BASE */

#define MUSB_FIFO(n)            (0x0000 + ((n) * 4))
#define MUSB_POWER              0x0040
#define MUSB_DEVCTL             0x0041
#define MUSB_INDEX              0x0042
#define MUSB_VEND0              0x0043
#define MUSB_INTRTX             0x0044
#define MUSB_INTRRX             0x0046
#define MUSB_INTRTXE            0x0048
#define MUSB_INTRRXE            0x004a
#define MUSB_INTRUSB            0x004c
#define MUSB_INTRUSBE           0x0050
#define MUSB_FRAME              0x0054
#define MUSB_TESTMODE           0x007c

/* Indexed EP registers (set MUSB_INDEX first) */

#define MUSB_TXMAXP             0x0080
#define MUSB_CSR0               0x0082
#define MUSB_TXCSR              0x0082
#define MUSB_RXMAXP             0x0084
#define MUSB_RXCSR              0x0086
#define MUSB_RXCOUNT            0x0088
#define MUSB_TXTYPE             0x008c
#define MUSB_TXINTERVAL         0x008d
#define MUSB_RXTYPE             0x008e
#define MUSB_RXINTERVAL         0x008f
#define MUSB_TXFIFOSZ           0x0090
#define MUSB_TXFIFOADD          0x0092
#define MUSB_RXFIFOSZ           0x0094
#define MUSB_RXFIFOADD          0x0096
#define MUSB_FADDR              0x0098

/* USB PHY registers (offsets from USB_PHY_BASE) */

#define USBPHY_ISCR             0x0000
#define USBPHY_PHYCTL40NM       0x0004
#define USBPHY_PHYBIST          0x0008
#define USBPHY_PHYCTL28NM       0x0010
#define USBPHY_PHYTEST          0x0014
#define USBPHY_PHYTUNE          0x0018
#define USBPHY_PHYSEL           0x0020
#define USBPHY_PHYSTA           0x0024

/* MUSB_POWER bits */

#define MUSB_POWER_SOFTCONN     (1 << 6)
#define MUSB_POWER_HSENAB       (1 << 5)
#define MUSB_POWER_HSMODE       (1 << 4)
#define MUSB_POWER_RESET        (1 << 3)
#define MUSB_POWER_RESUME       (1 << 2)
#define MUSB_POWER_SUSPENDM     (1 << 1)
#define MUSB_POWER_ENSUSPEND    (1 << 0)

/* MUSB_DEVCTL bits */

#define MUSB_DEVCTL_BDEVICE     (1 << 7)
#define MUSB_DEVCTL_FSDEV       (1 << 6)
#define MUSB_DEVCTL_LSDEV       (1 << 5)
#define MUSB_DEVCTL_VBUS_MASK   0x18
#define MUSB_DEVCTL_HOSTMODE    (1 << 2)
#define MUSB_DEVCTL_HOSTREQ     (1 << 1)
#define MUSB_DEVCTL_SESSION     (1 << 0)

/* MUSB_INTRUSB bits */

#define MUSB_INTR_SUSPEND       (1 << 0)
#define MUSB_INTR_RESUME        (1 << 1)
#define MUSB_INTR_RESET         (1 << 2)
#define MUSB_INTR_SOF           (1 << 3)
#define MUSB_INTR_CONNECT       (1 << 4)
#define MUSB_INTR_DISCONNECT    (1 << 5)
#define MUSB_INTR_SESSREQ       (1 << 6)
#define MUSB_INTR_VBUSERROR     (1 << 7)

/* MUSB_CSR0 bits (EP0) */

#define MUSB_CSR0_RXPKTRDY      (1 << 0)
#define MUSB_CSR0_TXPKTRDY      (1 << 1)
#define MUSB_CSR0_SENTSTALL     (1 << 2)
#define MUSB_CSR0_DATAEND       (1 << 3)
#define MUSB_CSR0_SETUPEND      (1 << 4)
#define MUSB_CSR0_SENDSTALL     (1 << 5)
#define MUSB_CSR0_SVDRXPKTRDY   (1 << 6)
#define MUSB_CSR0_SVDSETUPEND   (1 << 7)
#define MUSB_CSR0_FLUSHFIFO     (1 << 8)

/* MUSB_TXCSR bits */

#define MUSB_TXCSR_TXPKTRDY     (1 << 0)
#define MUSB_TXCSR_FIFONOTEMPTY (1 << 1)
#define MUSB_TXCSR_UNDERRUN     (1 << 2)
#define MUSB_TXCSR_FLUSHFIFO    (1 << 3)
#define MUSB_TXCSR_SENDSTALL    (1 << 4)
#define MUSB_TXCSR_SENTSTALL    (1 << 5)
#define MUSB_TXCSR_CLRDATATOG   (1 << 6)
#define MUSB_TXCSR_MODE         (1 << 13)
#define MUSB_TXCSR_AUTOSET      (1 << 15)

/* MUSB_RXCSR bits */

#define MUSB_RXCSR_RXPKTRDY     (1 << 0)
#define MUSB_RXCSR_FIFOFULL     (1 << 1)
#define MUSB_RXCSR_OVERRUN      (1 << 2)
#define MUSB_RXCSR_DATAERROR    (1 << 3)
#define MUSB_RXCSR_FLUSHFIFO    (1 << 4)
#define MUSB_RXCSR_SENDSTALL    (1 << 5)
#define MUSB_RXCSR_SENTSTALL    (1 << 6)
#define MUSB_RXCSR_CLRDATATOG   (1 << 7)
#define MUSB_RXCSR_AUTOCLEAR    (1 << 15)

/* FIFO size encoding */

#define MUSB_FIFOSZ_8           0
#define MUSB_FIFOSZ_16          1
#define MUSB_FIFOSZ_32          2
#define MUSB_FIFOSZ_64          3
#define MUSB_FIFOSZ_128         4
#define MUSB_FIFOSZ_256         5
#define MUSB_FIFOSZ_512         6
#define MUSB_FIFOSZ_1024        7
#define MUSB_FIFOSZ_2048        8

/* USB_ISCR bits */

#define USB_ISCR_DPDM_PULLUP_EN  (1 << 16)
#define USB_ISCR_ID_PULLUP_EN    (1 << 17)
#define USB_ISCR_FORCE_ID_MASK   0xc000
#define USB_ISCR_FORCE_ID_HIGH   0xc000
#define USB_ISCR_FORCE_VBUS_MASK 0x3000
#define USB_ISCR_FORCE_VBUS_HIGH 0x3000

/* USB_PHYCTL28NM bits */

#define USB_PHYCTL28NM_SIDDQ    (1 << 3)

/* USB_PHYSEL bits */

#define USB_PHYSEL_OTG_SEL      (1 << 0)

/* IRQ numbers */

#define T113_IRQ_USB0_DEVICE    61
#define T113_IRQ_USB0_EHCI      62
#define T113_IRQ_USB0_OHCI      63

/* EP count and FIFO */

#define T113_USB_NEPS           5
#define T113_USB_FIFO_SIZE      8192

#endif /* __ARCH_ARM_SRC_T113_HARDWARE_T113_USB_H */
