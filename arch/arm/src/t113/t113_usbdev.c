/****************************************************************************
 * arch/arm/src/t113/t113_usbdev.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/usb/usb.h>
#include <nuttx/usb/usbdev.h>
#include <nuttx/usb/usbdev_trace.h>

#include "arm_internal.h"
#include "hardware/t113_usb.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* USB OTG register base */

#define MUSB_BASE            T113_USB_OTG_BASE
#define PHY_BASE             T113_USB_PHY_BASE

/* Number of physical endpoints (EP0-EP4) */

#define T113_NPHYSEP         T113_USB_NEPS
#define T113_NLOGEP          (T113_NPHYSEP * 2)  /* IN + OUT for each */

/* EP0 max packet size */

#define EP0_MAXPACKET        64

/* Endpoint bitmask helpers */

#define T113_EPALLSET        0x1ff  /* EP0-EP4, both directions */
#define T113_EPCTRLSET       0x003  /* EP0 IN + OUT */

/* Debug tracing */

#ifdef CONFIG_DEBUG_USB_INFO
#  define usb_trace_info(fmt, ...) uinfo(fmt, ##__VA_ARGS__)
#else
#  define usb_trace_info(fmt, ...)
#endif

#ifdef CONFIG_DEBUG_USB_ERROR
#  define usb_trace_err(fmt, ...)  uerr(fmt, ##__VA_ARGS__)
#else
#  define usb_trace_err(fmt, ...)
#endif

/* Logical EP address encoding (matches NuttX convention) */

#define T113_EPPHYIN(n)      ((n) * 2)
#define T113_EPPHYOUT(n)     ((n) * 2 + 1)
#define T113_EP0_IN          T113_EPPHYIN(0)
#define T113_EP0_OUT         T113_EPPHYOUT(0)

/* Convert physical index to logical address */

#define PHYIN2LOG(n)         ((n) | USB_DIR_IN)
#define PHYOUT2LOG(n)        ((n) | USB_DIR_OUT)

/* Register access helpers */

#define musb_getreg8(off)    getreg8(MUSB_BASE + (off))
#define musb_putreg8(v, off) putreg8((v), MUSB_BASE + (off))
#define musb_getreg16(off)   getreg16(MUSB_BASE + (off))
#define musb_putreg16(v, off) putreg16((v), MUSB_BASE + (off))
#define musb_getreg32(off)   getreg32(MUSB_BASE + (off))
#define musb_putreg32(v, off) putreg32((v), MUSB_BASE + (off))

#define phy_getreg32(off)    getreg32(PHY_BASE + (off))
#define phy_putreg32(v, off) putreg32((v), PHY_BASE + (off))

/* Modify helpers */

#define musb_setbits8(off, bits) \
  musb_putreg8(musb_getreg8(off) | (bits), (off))
#define musb_clrbits8(off, bits) \
  musb_putreg8(musb_getreg8(off) & ~(bits), (off))
#define musb_setbits16(off, bits) \
  musb_putreg16(musb_getreg16(off) | (bits), (off))
#define musb_clrbits16(off, bits) \
  musb_putreg16(musb_getreg16(off) & ~(bits), (off))

#define phy_setbits32(off, bits) \
  phy_putreg32(phy_getreg32(off) | (bits), (off))
#define phy_clrbits32(off, bits) \
  phy_putreg32(phy_getreg32(off) & ~(bits), (off))

/* FIFO address is in units of 8 bytes */

#define FIFO_ADDR(bytes)     ((bytes) >> 3)

/* ISCR change detect bits (not in t113_usb.h, from vendor reference) */

#define USB_ISCR_VBUS_CHANGE_DETECT   0x0040
#define USB_ISCR_ID_CHANGE_DETECT     0x0020
#define USB_ISCR_DPDM_CHANGE_DETECT   0x0010

/* Debug trace variables — read via JLink mem32 */

volatile uint32_t g_usb_irq_count;
volatile uint32_t g_usb_last_usbintr;
volatile uint32_t g_usb_last_txintr;
volatile uint32_t g_usb_last_rxintr;
volatile uint32_t g_usb_reset_count;
volatile uint32_t g_usb_ep0_count;
volatile uint32_t g_usb_ep0_rxpktrdy;
volatile uint32_t g_usb_ep0_state;
volatile uint32_t g_usb_setup_type;
volatile uint32_t g_usb_setup_req;
volatile uint32_t g_usb_setup_value;
volatile uint32_t g_usb_setup_len;
volatile uint32_t g_usb_class_ret;
volatile uint32_t g_usb_ep0_submit;
volatile uint32_t g_usb_ep0_txpktrdy;
volatile uint32_t g_usb_ep0_csr0;

/* EP0 state machine */

#define EP0STATE_IDLE             0   /* Idle, waiting for SETUP */
#define EP0STATE_SETUP_OUT        1   /* OUT SETUP received (no data) */
#define EP0STATE_SETUP_IN         2   /* IN data requested by host */
#define EP0STATE_DATA_IN          3   /* Sending IN data */
#define EP0STATE_DATA_OUT         4   /* Receiving OUT data */
#define EP0STATE_SHORTWRITE       5   /* Short write, no req queued */
#define EP0STATE_WAIT_STATUS_IN   6   /* Waiting for IN status phase */
#define EP0STATE_WAIT_STATUS_OUT  7   /* Waiting for OUT status phase */
#define EP0STATE_STALL            8   /* Stalled */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* FIFO configuration for each endpoint direction */

struct t113_fifoconfig_s
{
  uint8_t  epno;        /* Physical EP number */
  bool     is_in;       /* true = TX/IN, false = RX/OUT */
  uint16_t addr;        /* FIFO start address (bytes) */
  uint16_t size;        /* FIFO size (bytes) */
  uint8_t  fifosz;      /* MUSB size encoding */
  bool     dpb;         /* Double-packet buffer */
};

/* Request wrapper */

struct t113_req_s
{
  struct usbdev_req_s  req;    /* Standard USB request (must be first) */
  struct t113_req_s   *flink;  /* Singly-linked list next pointer */
};

/* Endpoint state */

struct t113_ep_s
{
  struct usbdev_ep_s       ep;       /* Standard EP (must be first) */
  struct t113_usbdev_s    *dev;      /* Back-pointer to device */
  struct t113_req_s       *head;     /* Request queue head */
  struct t113_req_s       *tail;     /* Request queue tail */
  uint8_t                  epphy;    /* Physical EP number (0-4) */
  bool                     is_in;    /* Direction: true=IN/TX */
  uint8_t                  eptype;   /* Transfer type */
  bool                     stalled;  /* Endpoint stalled */
  uint16_t                 fifosz;   /* FIFO size */
};

/* Device state */

struct t113_usbdev_s
{
  struct usbdev_s              usbdev;    /* Standard device (must be first) */
  struct usbdevclass_driver_s *driver;    /* Bound class driver */

  /* EP0 control transfer state */

  uint8_t  ep0state;                      /* EP0 state machine */
  uint8_t  ep0buf[EP0_MAXPACKET];         /* EP0 data buffer */
  uint16_t ep0datlen;                     /* EP0 data length */
  uint16_t ep0reqlen;                     /* EP0 request wLength */

  /* Device state */

  uint8_t  paddr;                         /* USB address */
  bool     paddrset;                      /* Address pending */
  bool     selfpowered;                   /* Self-powered flag */
  bool     attached;                      /* Host attached */
  bool     suspended;                     /* Suspended */

  /* Setup request buffer */

  struct usb_ctrlreq_s  ep0ctrl;          /* Last SETUP packet */

  /* Available endpoints bitmap */

  uint32_t  epavail;

  /* All endpoint structures: EP0 IN/OUT + EP1-4 IN + EP1-4 OUT = 10 */

  struct t113_ep_s  eplist[T113_NLOGEP];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Request queue management */

static bool t113_rqempty(struct t113_ep_s *privep);
static struct t113_req_s *t113_rqpeek(struct t113_ep_s *privep);
static struct t113_req_s *t113_rqdequeue(struct t113_ep_s *privep);
static bool t113_rqenqueue(struct t113_ep_s *privep,
                           struct t113_req_s *privreq);

/* Low-level MUSB register access */

static void t113_ep_select(uint8_t epno);
static void t113_fifo_read(uint8_t epno, uint8_t *buf, uint16_t len);
static void t113_fifo_write(uint8_t epno, const uint8_t *buf, uint16_t len);

/* Request processing */

static void t113_reqcomplete(struct t113_ep_s *privep,
                             struct t113_req_s *privreq,
                             int16_t result);
static void t113_cancelrequests(struct t113_ep_s *privep,
                                int16_t status);

/* EP0 handling */

static void t113_ep0_setup(struct t113_usbdev_s *priv);
static void t113_ep0_indone(struct t113_usbdev_s *priv);
static void t113_ep0_outdone(struct t113_usbdev_s *priv);
static void t113_ep0_dispatch(struct t113_usbdev_s *priv);
static void t113_ep0_transmit(struct t113_usbdev_s *priv,
                              const uint8_t *buf, uint16_t len);

/* EPn handling */

static void t113_epn_txdone(struct t113_usbdev_s *priv, uint8_t epno);
static void t113_epn_rxready(struct t113_usbdev_s *priv, uint8_t epno);
static void t113_epn_txstart(struct t113_ep_s *privep);

/* Interrupt handling */

static int  t113_usbdev_interrupt(int irq, void *context, void *arg);

/* Hardware init / deinit */

static void t113_ccu_init(void);
static void t113_phy_init(void);
static void t113_musb_init(struct t113_usbdev_s *priv);
static void t113_musb_enable(void);
static void t113_musb_reset(struct t113_usbdev_s *priv);

/* Endpoint operations (usbdev_epops_s) */

static int  t113_epconfigure(struct usbdev_ep_s *ep,
                             const struct usb_epdesc_s *desc, bool last);
static int  t113_epdisable(struct usbdev_ep_s *ep);
static struct usbdev_req_s *t113_epallocreq(struct usbdev_ep_s *ep);
static void t113_epfreereq(struct usbdev_ep_s *ep,
                           struct usbdev_req_s *req);
static int  t113_epsubmit(struct usbdev_ep_s *ep,
                          struct usbdev_req_s *req);
static int  t113_epcancel(struct usbdev_ep_s *ep,
                          struct usbdev_req_s *req);
static int  t113_epstall(struct usbdev_ep_s *ep, bool resume);

/* Device operations (usbdev_ops_s) */

static struct usbdev_ep_s *t113_allocep(struct usbdev_s *dev,
                                        uint8_t epphy, bool in,
                                        uint8_t eptype);
static void t113_freeep(struct usbdev_s *dev, struct usbdev_ep_s *ep);
static int  t113_getframe(struct usbdev_s *dev);
static int  t113_wakeup(struct usbdev_s *dev);
static int  t113_selfpowered(struct usbdev_s *dev, bool selfpowered);
static int  t113_pullup(struct usbdev_s *dev, bool enable);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Endpoint operations */

static const struct usbdev_epops_s g_epops =
{
  .configure  = t113_epconfigure,
  .disable    = t113_epdisable,
  .allocreq   = t113_epallocreq,
  .freereq    = t113_epfreereq,
  .submit     = t113_epsubmit,
  .cancel     = t113_epcancel,
  .stall      = t113_epstall,
};

/* Device operations */

static const struct usbdev_ops_s g_devops =
{
  .allocep      = t113_allocep,
  .freeep       = t113_freeep,
  .getframe     = t113_getframe,
  .wakeup       = t113_wakeup,
  .selfpowered  = t113_selfpowered,
  .pullup       = t113_pullup,
};

/* Single device instance */

static struct t113_usbdev_s g_usbdev;

/* FIFO layout for T113 (8KB total, EP0-EP4):
 *
 * EP0:       64B  (offset 0)
 * EP1 TX: 1024B  (offset 64)    double-buffered
 * EP1 RX: 1024B  (offset 1088)  double-buffered
 * EP2 TX: 1024B  (offset 2112)  double-buffered
 * EP2 RX: 1024B  (offset 3136)  double-buffered
 * EP3 TX:  512B  (offset 4160)
 * EP3 RX:  512B  (offset 4672)
 * EP4 TX:  512B  (offset 5184)
 * EP4 RX:  512B  (offset 5696)
 * Total:  6208B used of 8192B available
 */

static const struct t113_fifoconfig_s g_fifoconfig[] =
{
  /* EP0: 64 bytes, single buffer at offset 0 */

  { 0, true,     0,   64, MUSB_FIFOSZ_64,   false },

  /* EP1: 1024 bytes each direction, double-buffered */

  { 1, true,    64, 1024, MUSB_FIFOSZ_1024,  true },
  { 1, false, 1088, 1024, MUSB_FIFOSZ_1024,  true },

  /* EP2: 1024 bytes each direction, double-buffered */

  { 2, true,  2112, 1024, MUSB_FIFOSZ_1024,  true },
  { 2, false, 3136, 1024, MUSB_FIFOSZ_1024,  true },

  /* EP3: 512 bytes each direction, single buffer */

  { 3, true,  4160,  512, MUSB_FIFOSZ_512,   false },
  { 3, false, 4672,  512, MUSB_FIFOSZ_512,   false },

  /* EP4: 512 bytes each direction, single buffer */

  { 4, true,  5184,  512, MUSB_FIFOSZ_512,   false },
  { 4, false, 5696,  512, MUSB_FIFOSZ_512,   false },
};

#define NFIFOCONFIGS (sizeof(g_fifoconfig) / sizeof(g_fifoconfig[0]))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: t113_rqempty
 *
 * Description:
 *   Check if the request queue for an endpoint is empty.
 *
 ****************************************************************************/

static bool t113_rqempty(struct t113_ep_s *privep)
{
  return privep->head == NULL;
}

/****************************************************************************
 * Name: t113_rqpeek
 *
 * Description:
 *   Return the head request without removing it.
 *
 ****************************************************************************/

static struct t113_req_s *t113_rqpeek(struct t113_ep_s *privep)
{
  return privep->head;
}

/****************************************************************************
 * Name: t113_rqdequeue
 *
 * Description:
 *   Dequeue the head request.
 *
 ****************************************************************************/

static struct t113_req_s *t113_rqdequeue(struct t113_ep_s *privep)
{
  struct t113_req_s *ret = privep->head;

  if (ret != NULL)
    {
      privep->head = ret->flink;
      if (privep->head == NULL)
        {
          privep->tail = NULL;
        }

      ret->flink = NULL;
    }

  return ret;
}

/****************************************************************************
 * Name: t113_rqenqueue
 *
 * Description:
 *   Enqueue a request.  Returns true if the queue was empty before.
 *
 ****************************************************************************/

static bool t113_rqenqueue(struct t113_ep_s *privep,
                           struct t113_req_s *privreq)
{
  bool was_empty = (privep->head == NULL);

  privreq->flink = NULL;
  if (was_empty)
    {
      privep->head = privreq;
      privep->tail = privreq;
    }
  else
    {
      privep->tail->flink = privreq;
      privep->tail = privreq;
    }

  return was_empty;
}

/****************************************************************************
 * Name: t113_ep_select
 *
 * Description:
 *   Select an endpoint via the MUSB INDEX register.
 *
 ****************************************************************************/

static void t113_ep_select(uint8_t epno)
{
  musb_putreg8(epno, MUSB_INDEX);
}

/****************************************************************************
 * Name: t113_fifo_read
 *
 * Description:
 *   Read data from the MUSB FIFO for the given endpoint.
 *
 ****************************************************************************/

static void t113_fifo_read(uint8_t epno, uint8_t *buf, uint16_t len)
{
  uint32_t fifo_addr = MUSB_BASE + MUSB_FIFO(epno);
  uint32_t count32;
  uint16_t i;

  /* Read 32-bit words first for efficiency */

  count32 = len >> 2;
  for (i = 0; i < count32; i++)
    {
      *((uint32_t *)buf) = getreg32(fifo_addr);
      buf += 4;
    }

  /* Read remaining bytes */

  len &= 3;
  for (i = 0; i < len; i++)
    {
      *buf++ = getreg8(fifo_addr);
    }
}

/****************************************************************************
 * Name: t113_fifo_write
 *
 * Description:
 *   Write data to the MUSB FIFO for the given endpoint.
 *
 ****************************************************************************/

static void t113_fifo_write(uint8_t epno, const uint8_t *buf, uint16_t len)
{
  uint32_t fifo_addr = MUSB_BASE + MUSB_FIFO(epno);
  uint32_t count32;
  uint16_t i;

  /* Write 32-bit words first for efficiency */

  count32 = len >> 2;
  for (i = 0; i < count32; i++)
    {
      putreg32(*((const uint32_t *)buf), fifo_addr);
      buf += 4;
    }

  /* Write remaining bytes */

  len &= 3;
  for (i = 0; i < len; i++)
    {
      putreg8(*buf++, fifo_addr);
    }
}

/****************************************************************************
 * Name: t113_reqcomplete
 *
 * Description:
 *   Complete a request and invoke the callback.
 *
 ****************************************************************************/

static void t113_reqcomplete(struct t113_ep_s *privep,
                             struct t113_req_s *privreq,
                             int16_t result)
{
  bool stalled = privep->stalled;

  /* For EP0, propagate the device stall state */

  if (privep->epphy == 0)
    {
      privep->stalled = privep->dev->ep0state == EP0STATE_STALL;
    }

  privreq->req.result = result;

  usb_trace_info("EP%d %s complete: len=%d xfrd=%d result=%d\n",
                 privep->epphy, privep->is_in ? "IN" : "OUT",
                 privreq->req.len, privreq->req.xfrd, result);

  /* Invoke the completion callback */

  privreq->req.callback(&privep->ep, &privreq->req);

  /* Restore the stall state */

  privep->stalled = stalled;
}

/****************************************************************************
 * Name: t113_cancelrequests
 *
 * Description:
 *   Cancel all queued requests on an endpoint.
 *
 ****************************************************************************/

static void t113_cancelrequests(struct t113_ep_s *privep,
                                int16_t status)
{
  while (!t113_rqempty(privep))
    {
      struct t113_req_s *privreq = t113_rqdequeue(privep);
      t113_reqcomplete(privep, privreq, status);
    }
}

/****************************************************************************
 * Name: t113_ep0_transmit
 *
 * Description:
 *   Start an EP0 IN data transfer.
 *
 ****************************************************************************/

static void t113_ep0_transmit(struct t113_usbdev_s *priv,
                              const uint8_t *buf, uint16_t len)
{
  uint16_t xfrlen;
  uint16_t csr0;

  t113_ep_select(0);

  /* Limit to max packet and requested length */

  xfrlen = len;
  if (xfrlen > EP0_MAXPACKET)
    {
      xfrlen = EP0_MAXPACKET;
    }

  if (xfrlen > priv->ep0reqlen)
    {
      xfrlen = priv->ep0reqlen;
    }

  /* Write data to EP0 FIFO */

  if (xfrlen > 0)
    {
      t113_fifo_write(0, buf, xfrlen);
    }

  priv->ep0datlen -= xfrlen;
  priv->ep0reqlen -= xfrlen;

  /* Set TXPKTRDY. If this is the last packet, set DATAEND too. */

  csr0 = MUSB_CSR0_TXPKTRDY;
  if (xfrlen < EP0_MAXPACKET || priv->ep0datlen == 0 ||
      priv->ep0reqlen == 0)
    {
      /* This is the last packet */

      csr0 |= MUSB_CSR0_DATAEND;
      priv->ep0state = EP0STATE_WAIT_STATUS_OUT;
    }
  else
    {
      priv->ep0state = EP0STATE_DATA_IN;
    }

  musb_putreg16(csr0, MUSB_CSR0);
}

/****************************************************************************
 * Name: t113_ep0_dispatch
 *
 * Description:
 *   Dispatch a SETUP packet to the class driver.
 *
 ****************************************************************************/

static void t113_ep0_dispatch(struct t113_usbdev_s *priv)
{
  int ret;
  g_usb_class_ret = 0xdead;

  if (priv->driver == NULL)
    {
      return;
    }

  ret = CLASS_SETUP(priv->driver, &priv->usbdev,
                    &priv->ep0ctrl, priv->ep0buf, priv->ep0datlen);
  g_usb_class_ret = (uint32_t)ret;
  if (ret < 0)
    {
      /* Stall EP0 on error */

      usb_trace_err("CLASS_SETUP failed: %d\n", ret);
      priv->ep0state = EP0STATE_STALL;
      t113_ep_select(0);
      musb_putreg16(MUSB_CSR0_SENDSTALL, MUSB_CSR0);
    }
  else if (ret == 0 && USB_REQ_ISOUT(priv->ep0ctrl.type) &&
           GETUINT16(priv->ep0ctrl.len) == 0)
    {
      /* No-data OUT control transfer completed */

      t113_ep_select(0);
      musb_putreg16(MUSB_CSR0_SVDRXPKTRDY | MUSB_CSR0_DATAEND, MUSB_CSR0);
      priv->ep0state = EP0STATE_IDLE;
    }
}

/****************************************************************************
 * Name: t113_ep0_stdrequest
 *
 * Description:
 *   Handle standard EP0 control requests (SET_ADDRESS etc).
 *   Returns true if handled, false if the class driver should handle it.
 *
 ****************************************************************************/

static bool t113_ep0_stdrequest(struct t113_usbdev_s *priv,
                                const struct usb_ctrlreq_s *ctrl)
{
  uint16_t value  = GETUINT16(ctrl->value);
  uint16_t len    = GETUINT16(ctrl->len);
  bool handled    = false;

  /* Only handle standard device requests */

  if ((ctrl->type & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_STANDARD)
    {
      return false;
    }

  switch (ctrl->req)
    {
      case USB_REQ_SETADDRESS:
        {
          /* Save address, will be set after status phase */

          priv->paddr = (uint8_t)(value & 0x7f);
          priv->paddrset = true;

          /* Send zero-length status */

          t113_ep_select(0);
          musb_putreg16(MUSB_CSR0_SVDRXPKTRDY | MUSB_CSR0_DATAEND,
                        MUSB_CSR0);
          priv->ep0state = EP0STATE_WAIT_STATUS_IN;
          handled = true;

          usb_trace_info("SET_ADDRESS: %d\n", priv->paddr);
        }
        break;

      case USB_REQ_GETSTATUS:
        {
          uint8_t recipient = ctrl->type & USB_REQ_RECIPIENT_MASK;
          uint16_t status_val = 0;

          if (recipient == USB_REQ_RECIPIENT_DEVICE)
            {
              status_val = priv->selfpowered ?
                           USB_FEATURE_SELFPOWERED : 0;
            }
          else if (recipient == USB_REQ_RECIPIENT_ENDPOINT)
            {
              /* Check stall status of the addressed endpoint */

              uint8_t epno = USB_EPNO(GETUINT16(ctrl->index));

              if (epno < T113_NPHYSEP)
                {
                  /* Check both IN and OUT for this EP number */

                  int idx = USB_ISEPIN(GETUINT16(ctrl->index)) ?
                            T113_EPPHYIN(epno) : T113_EPPHYOUT(epno);
                  if (priv->eplist[idx].stalled)
                    {
                      status_val = 1;
                    }
                }
            }

          priv->ep0buf[0] = (uint8_t)(status_val & 0xff);
          priv->ep0buf[1] = (uint8_t)((status_val >> 8) & 0xff);

          t113_ep_select(0);
          musb_putreg16(MUSB_CSR0_SVDRXPKTRDY, MUSB_CSR0);

          priv->ep0datlen = 2;
          priv->ep0reqlen = len;
          t113_ep0_transmit(priv, priv->ep0buf, 2);
          handled = true;
        }
        break;

      case USB_REQ_CLEARFEATURE:
        {
          if ((ctrl->type & USB_REQ_RECIPIENT_MASK) ==
              USB_REQ_RECIPIENT_ENDPOINT &&
              value == USB_FEATURE_ENDPOINTHALT)
            {
              uint8_t epno = USB_EPNO(GETUINT16(ctrl->index));
              bool is_in   = USB_ISEPIN(GETUINT16(ctrl->index));

              if (epno > 0 && epno < T113_NPHYSEP)
                {
                  int idx = is_in ? T113_EPPHYIN(epno) :
                                    T113_EPPHYOUT(epno);
                  struct t113_ep_s *privep = &priv->eplist[idx];

                  /* Clear the stall */

                  privep->stalled = false;
                  t113_ep_select(epno);

                  if (is_in)
                    {
                      musb_clrbits16(MUSB_TXCSR,
                                     MUSB_TXCSR_SENDSTALL |
                                     MUSB_TXCSR_SENTSTALL);
                      musb_setbits16(MUSB_TXCSR,
                                     MUSB_TXCSR_CLRDATATOG);
                    }
                  else
                    {
                      musb_clrbits16(MUSB_RXCSR,
                                     MUSB_RXCSR_SENDSTALL |
                                     MUSB_RXCSR_SENTSTALL);
                      musb_setbits16(MUSB_RXCSR,
                                     MUSB_RXCSR_CLRDATATOG);
                    }
                }

              t113_ep_select(0);
              musb_putreg16(MUSB_CSR0_SVDRXPKTRDY | MUSB_CSR0_DATAEND,
                            MUSB_CSR0);
              priv->ep0state = EP0STATE_IDLE;
              handled = true;
            }
        }
        break;

      case USB_REQ_SETFEATURE:
        {
          if ((ctrl->type & USB_REQ_RECIPIENT_MASK) ==
              USB_REQ_RECIPIENT_ENDPOINT &&
              value == USB_FEATURE_ENDPOINTHALT)
            {
              uint8_t epno = USB_EPNO(GETUINT16(ctrl->index));
              bool is_in   = USB_ISEPIN(GETUINT16(ctrl->index));

              if (epno > 0 && epno < T113_NPHYSEP)
                {
                  int idx = is_in ? T113_EPPHYIN(epno) :
                                    T113_EPPHYOUT(epno);
                  struct t113_ep_s *privep = &priv->eplist[idx];

                  privep->stalled = true;
                  t113_ep_select(epno);

                  if (is_in)
                    {
                      musb_setbits16(MUSB_TXCSR,
                                     MUSB_TXCSR_SENDSTALL);
                    }
                  else
                    {
                      musb_setbits16(MUSB_RXCSR,
                                     MUSB_RXCSR_SENDSTALL);
                    }
                }

              t113_ep_select(0);
              musb_putreg16(MUSB_CSR0_SVDRXPKTRDY | MUSB_CSR0_DATAEND,
                            MUSB_CSR0);
              priv->ep0state = EP0STATE_IDLE;
              handled = true;
            }
        }
        break;

      default:
        break;
    }

  return handled;
}

/****************************************************************************
 * Name: t113_ep0_setup
 *
 * Description:
 *   Handle EP0 SETUP packet reception.
 *
 ****************************************************************************/

static void t113_ep0_setup(struct t113_usbdev_s *priv)
{
  struct usb_ctrlreq_s ctrl;
  uint16_t csr0;

  t113_ep_select(0);
  csr0 = musb_getreg16(MUSB_CSR0);

  if (csr0 & MUSB_CSR0_SENTSTALL)
    {
      musb_clrbits16(MUSB_CSR0, MUSB_CSR0_SENDSTALL);
      musb_clrbits16(MUSB_CSR0, MUSB_CSR0_SENTSTALL);
      priv->ep0state = EP0STATE_IDLE;
      return;
    }

  if (csr0 & MUSB_CSR0_SETUPEND)
    {
      musb_putreg16(MUSB_CSR0_SVDSETUPEND, MUSB_CSR0);
      priv->ep0state = EP0STATE_IDLE;
    }

  /* Check for RXPKTRDY (SETUP packet available) */

  csr0 = musb_getreg16(MUSB_CSR0);
  if (!(csr0 & MUSB_CSR0_RXPKTRDY))
    {
      /* No SETUP packet, handle ongoing transfers */

      switch (priv->ep0state)
        {
          case EP0STATE_DATA_IN:
            t113_ep0_indone(priv);
            break;

          case EP0STATE_DATA_OUT:
            t113_ep0_outdone(priv);
            break;

          case EP0STATE_WAIT_STATUS_IN:
            /* Status phase complete, apply pending address if needed */

            if (priv->paddrset)
              {
                musb_putreg8(priv->paddr, MUSB_FADDR);
                priv->paddrset = false;
                usb_trace_info("FADDR set to %d\n", priv->paddr);
              }

            priv->ep0state = EP0STATE_IDLE;
            break;

          case EP0STATE_WAIT_STATUS_OUT:
            priv->ep0state = EP0STATE_IDLE;
            break;

          default:
            break;
        }

      return;
    }

  /* Read the 8-byte SETUP packet from EP0 FIFO */

  t113_fifo_read(0, (uint8_t *)&ctrl, USB_SIZEOF_CTRLREQ);
  memcpy(&priv->ep0ctrl, &ctrl, USB_SIZEOF_CTRLREQ);

  g_usb_ep0_rxpktrdy++;
  g_usb_setup_type = ctrl.type;
  g_usb_setup_req = ctrl.req;
  g_usb_setup_value = GETUINT16(ctrl.value);
  g_usb_setup_len = GETUINT16(ctrl.len);

  usb_trace_info("SETUP: type=0x%02x req=0x%02x val=0x%04x "
                 "idx=0x%04x len=0x%04x\n",
                 ctrl.type, ctrl.req,
                 GETUINT16(ctrl.value),
                 GETUINT16(ctrl.index),
                 GETUINT16(ctrl.len));

  priv->ep0datlen = 0;
  priv->ep0reqlen = GETUINT16(ctrl.len);

  /* Try standard request handling first */

  if (t113_ep0_stdrequest(priv, &ctrl))
    {
      return;
    }

  /* Service RXPKTRDY for class driver dispatch */

  musb_putreg16(MUSB_CSR0_SVDRXPKTRDY, MUSB_CSR0);

  /* Not a standard request, dispatch to class driver */

  if (USB_REQ_ISOUT(ctrl.type) && priv->ep0reqlen > 0)
    {
      /* OUT data phase expected */

      priv->ep0state = EP0STATE_DATA_OUT;
    }
  else if (USB_REQ_ISIN(ctrl.type))
    {
      /* IN data phase expected */

      priv->ep0state = EP0STATE_SETUP_IN;
    }
  else
    {
      /* No data phase */

      priv->ep0state = EP0STATE_SETUP_OUT;
    }

  t113_ep0_dispatch(priv);
}

/****************************************************************************
 * Name: t113_ep0_indone
 *
 * Description:
 *   EP0 IN transfer completed (previous TXPKTRDY was sent).
 *   Continue sending data if more is available.
 *
 ****************************************************************************/

static void t113_ep0_indone(struct t113_usbdev_s *priv)
{
  struct t113_ep_s *ep0in = &priv->eplist[T113_EP0_IN];
  struct t113_req_s *privreq;

  /* Check if there's a queued request */

  privreq = t113_rqpeek(ep0in);
  if (privreq != NULL)
    {
      uint16_t remaining = privreq->req.len - privreq->req.xfrd;

      if (remaining > 0 && priv->ep0reqlen > 0)
        {
          uint16_t xfrlen = remaining;
          if (xfrlen > EP0_MAXPACKET)
            {
              xfrlen = EP0_MAXPACKET;
            }

          if (xfrlen > priv->ep0reqlen)
            {
              xfrlen = priv->ep0reqlen;
            }

          t113_ep_select(0);
          t113_fifo_write(0,
                          privreq->req.buf + privreq->req.xfrd,
                          xfrlen);
          privreq->req.xfrd += xfrlen;
          priv->ep0reqlen -= xfrlen;

          if (xfrlen < EP0_MAXPACKET || privreq->req.xfrd >=
              privreq->req.len || priv->ep0reqlen == 0)
            {
              musb_putreg16(MUSB_CSR0_TXPKTRDY | MUSB_CSR0_DATAEND,
                            MUSB_CSR0);
              priv->ep0state = EP0STATE_WAIT_STATUS_OUT;

              /* Complete the request */

              privreq = t113_rqdequeue(ep0in);
              if (privreq != NULL)
                {
                  t113_reqcomplete(ep0in, privreq, OK);
                }
            }
          else
            {
              musb_putreg16(MUSB_CSR0_TXPKTRDY, MUSB_CSR0);
            }
        }
      else
        {
          /* No more data, complete the request */

          t113_ep_select(0);
          musb_putreg16(MUSB_CSR0_TXPKTRDY | MUSB_CSR0_DATAEND,
                        MUSB_CSR0);
          priv->ep0state = EP0STATE_WAIT_STATUS_OUT;

          privreq = t113_rqdequeue(ep0in);
          if (privreq != NULL)
            {
              t113_reqcomplete(ep0in, privreq, OK);
            }
        }
    }
  else
    {
      /* No request queued, short write from ep0buf completed */

      if (priv->ep0datlen > 0 && priv->ep0reqlen > 0)
        {
          t113_ep0_transmit(priv,
                            priv->ep0buf + (EP0_MAXPACKET -
                            priv->ep0datlen),
                            priv->ep0datlen);
        }
      else
        {
          priv->ep0state = EP0STATE_WAIT_STATUS_OUT;
        }
    }
}

/****************************************************************************
 * Name: t113_ep0_outdone
 *
 * Description:
 *   EP0 OUT data received.
 *
 ****************************************************************************/

static void t113_ep0_outdone(struct t113_usbdev_s *priv)
{
  struct t113_ep_s *ep0out = &priv->eplist[T113_EP0_OUT];
  struct t113_req_s *privreq;
  uint16_t rxcount;

  t113_ep_select(0);
  rxcount = musb_getreg16(MUSB_RXCOUNT);

  privreq = t113_rqpeek(ep0out);
  if (privreq != NULL)
    {
      uint16_t xfrlen = rxcount;
      uint16_t remaining = privreq->req.len - privreq->req.xfrd;

      if (xfrlen > remaining)
        {
          xfrlen = remaining;
        }

      t113_fifo_read(0, privreq->req.buf + privreq->req.xfrd, xfrlen);
      privreq->req.xfrd += xfrlen;

      if (privreq->req.xfrd >= privreq->req.len ||
          xfrlen < EP0_MAXPACKET)
        {
          /* Transfer complete */

          musb_putreg16(MUSB_CSR0_SVDRXPKTRDY | MUSB_CSR0_DATAEND,
                        MUSB_CSR0);
          priv->ep0state = EP0STATE_WAIT_STATUS_IN;

          privreq = t113_rqdequeue(ep0out);
          if (privreq != NULL)
            {
              t113_reqcomplete(ep0out, privreq, OK);
            }
        }
      else
        {
          musb_putreg16(MUSB_CSR0_SVDRXPKTRDY, MUSB_CSR0);
        }
    }
  else
    {
      /* No request, read into ep0buf */

      uint16_t xfrlen = rxcount;
      if (xfrlen > EP0_MAXPACKET)
        {
          xfrlen = EP0_MAXPACKET;
        }

      t113_fifo_read(0, priv->ep0buf, xfrlen);
      priv->ep0datlen = xfrlen;

      musb_putreg16(MUSB_CSR0_SVDRXPKTRDY | MUSB_CSR0_DATAEND,
                    MUSB_CSR0);
      priv->ep0state = EP0STATE_WAIT_STATUS_IN;

      /* Re-dispatch with data */

      t113_ep0_dispatch(priv);
    }
}

/****************************************************************************
 * Name: t113_epn_txstart
 *
 * Description:
 *   Start a TX transfer on a bulk/interrupt IN endpoint.
 *
 ****************************************************************************/

static void t113_epn_txstart(struct t113_ep_s *privep)
{
  struct t113_req_s *privreq;
  uint16_t xfrlen;

  privreq = t113_rqpeek(privep);
  if (privreq == NULL)
    {
      return;
    }

  /* Calculate transfer size */

  xfrlen = privreq->req.len - privreq->req.xfrd;
  if (xfrlen > privep->ep.maxpacket)
    {
      xfrlen = privep->ep.maxpacket;
    }

  /* Select EP and write to FIFO */

  t113_ep_select(privep->epphy);

  /* Make sure previous TX completed */

  if (musb_getreg16(MUSB_TXCSR) & MUSB_TXCSR_TXPKTRDY)
    {
      return;
    }

  t113_fifo_write(privep->epphy,
                  privreq->req.buf + privreq->req.xfrd,
                  xfrlen);
  privreq->req.xfrd += xfrlen;

  /* Set MODE (TX) and TXPKTRDY */

  musb_putreg16(MUSB_TXCSR_MODE | MUSB_TXCSR_TXPKTRDY, MUSB_TXCSR);
}

/****************************************************************************
 * Name: t113_epn_txdone
 *
 * Description:
 *   Handle TX complete interrupt for EPn.
 *
 ****************************************************************************/

static void t113_epn_txdone(struct t113_usbdev_s *priv, uint8_t epno)
{
  struct t113_ep_s *privep = &priv->eplist[T113_EPPHYIN(epno)];
  struct t113_req_s *privreq;
  uint16_t txcsr;

  t113_ep_select(epno);
  txcsr = musb_getreg16(MUSB_TXCSR);

  /* Clear SENTSTALL if set */

  if (txcsr & MUSB_TXCSR_SENTSTALL)
    {
      musb_clrbits16(MUSB_TXCSR, MUSB_TXCSR_SENTSTALL |
                                  MUSB_TXCSR_SENDSTALL);
      musb_setbits16(MUSB_TXCSR, MUSB_TXCSR_CLRDATATOG);
      return;
    }

  /* Clear underrun */

  if (txcsr & MUSB_TXCSR_UNDERRUN)
    {
      musb_clrbits16(MUSB_TXCSR, MUSB_TXCSR_UNDERRUN);
    }

  privreq = t113_rqpeek(privep);
  if (privreq == NULL)
    {
      return;
    }

  /* Check if transfer is complete */

  if (privreq->req.xfrd >= privreq->req.len)
    {
      /* Check if we need to send ZLP */

      bool need_zlp = (privreq->req.flags & USBDEV_REQFLAGS_NULLPKT) &&
                      (privreq->req.len > 0) &&
                      (privreq->req.len % privep->ep.maxpacket == 0);

      if (need_zlp)
        {
          /* Send ZLP */

          privreq->req.flags &= ~USBDEV_REQFLAGS_NULLPKT;
          musb_putreg16(MUSB_TXCSR_MODE | MUSB_TXCSR_TXPKTRDY,
                        MUSB_TXCSR);
          return;
        }

      /* Complete the request */

      privreq = t113_rqdequeue(privep);
      if (privreq != NULL)
        {
          t113_reqcomplete(privep, privreq, OK);
        }

      /* Start next request if any */

      t113_epn_txstart(privep);
    }
  else
    {
      /* More data to send */

      t113_epn_txstart(privep);
    }
}

/****************************************************************************
 * Name: t113_epn_rxready
 *
 * Description:
 *   Handle RX ready interrupt for EPn.
 *
 ****************************************************************************/

static void t113_epn_rxready(struct t113_usbdev_s *priv, uint8_t epno)
{
  struct t113_ep_s *privep = &priv->eplist[T113_EPPHYOUT(epno)];
  struct t113_req_s *privreq;
  uint16_t rxcsr;
  uint16_t rxcount;

  t113_ep_select(epno);
  rxcsr = musb_getreg16(MUSB_RXCSR);

  /* Clear SENTSTALL if set */

  if (rxcsr & MUSB_RXCSR_SENTSTALL)
    {
      musb_clrbits16(MUSB_RXCSR, MUSB_RXCSR_SENTSTALL |
                                  MUSB_RXCSR_SENDSTALL);
      musb_setbits16(MUSB_RXCSR, MUSB_RXCSR_CLRDATATOG);
      return;
    }

  /* Clear overrun */

  if (rxcsr & MUSB_RXCSR_OVERRUN)
    {
      musb_clrbits16(MUSB_RXCSR, MUSB_RXCSR_OVERRUN);
    }

  if (!(rxcsr & MUSB_RXCSR_RXPKTRDY))
    {
      return;
    }

  rxcount = musb_getreg16(MUSB_RXCOUNT);

  privreq = t113_rqpeek(privep);
  if (privreq == NULL)
    {
      /* No request pending; leave RXPKTRDY set to NAK host */

      usb_trace_info("EP%d OUT: no request, NAKing (%d bytes)\n",
                     epno, rxcount);
      return;
    }

  /* Read data from FIFO */

  uint16_t remaining = privreq->req.len - privreq->req.xfrd;
  uint16_t xfrlen = rxcount;

  if (xfrlen > remaining)
    {
      xfrlen = remaining;
    }

  t113_fifo_read(epno, privreq->req.buf + privreq->req.xfrd, xfrlen);
  privreq->req.xfrd += xfrlen;

  /* Clear RXPKTRDY */

  musb_clrbits16(MUSB_RXCSR, MUSB_RXCSR_RXPKTRDY);

  /* Check if transfer is complete: short packet or buffer full */

  if (xfrlen < privep->ep.maxpacket ||
      privreq->req.xfrd >= privreq->req.len)
    {
      privreq = t113_rqdequeue(privep);
      if (privreq != NULL)
        {
          t113_reqcomplete(privep, privreq, OK);
        }
    }
}

/****************************************************************************
 * Name: t113_musb_reset
 *
 * Description:
 *   Handle USB bus reset.
 *
 ****************************************************************************/

static void t113_musb_reset(struct t113_usbdev_s *priv)
{
  int i;

  t113_ep_select(0);
  g_usb_ep0_csr0 = musb_getreg16(MUSB_CSR0);

  musb_putreg8(0, MUSB_FADDR);
  priv->paddr = 0;
  priv->paddrset = false;
  priv->ep0state = EP0STATE_IDLE;
  priv->ep0datlen = 0;
  priv->ep0reqlen = 0;

  t113_ep_select(0);
  musb_putreg16(MUSB_CSR0_FLUSHFIFO, MUSB_CSR0);
  musb_putreg16(MUSB_CSR0_SVDSETUPEND | MUSB_CSR0_SVDRXPKTRDY,
                MUSB_CSR0);

  for (i = 0; i < T113_NLOGEP; i++)
    {
      t113_cancelrequests(&priv->eplist[i], -ECONNRESET);
    }

  priv->usbdev.speed = USB_SPEED_FULL;

  t113_ep_select(0);

  musb_putreg16(1, MUSB_INTRTXE);
  musb_putreg32(MUSB_INTR_SUSPEND | MUSB_INTR_RESUME | MUSB_INTR_RESET,
               MUSB_INTRUSBE);

  if (priv->driver != NULL)
    {
      CLASS_DISCONNECT(priv->driver, &priv->usbdev);
    }
}

/****************************************************************************
 * Name: t113_usbdev_interrupt
 *
 * Description:
 *   USB interrupt handler.
 *
 ****************************************************************************/

static int t113_usbdev_interrupt(int irq, void *context, void *arg)
{
  struct t113_usbdev_s *priv = (struct t113_usbdev_s *)arg;
  uint8_t  usbintr;
  uint16_t txintr;
  uint16_t rxintr;
  uint8_t  old_index;
  int      i;

  UNUSED(irq);
  UNUSED(context);

  /* Save current EP index */

  old_index = musb_getreg8(MUSB_INDEX);

  /* Read and clear interrupt status registers */

  usbintr = musb_getreg32(MUSB_INTRUSB) & 0xff;
  txintr  = musb_getreg16(MUSB_INTRTX);
  rxintr  = musb_getreg16(MUSB_INTRRX);

  g_usb_irq_count++;
  g_usb_last_usbintr = usbintr;
  g_usb_last_txintr = txintr;
  g_usb_last_rxintr = rxintr;

  /* Clear interrupt status (write-1-to-clear) */

  if (usbintr)
    {
      musb_putreg32(usbintr, MUSB_INTRUSB);
    }

  if (txintr)
    {
      musb_putreg16(txintr, MUSB_INTRTX);
    }

  if (rxintr)
    {
      musb_putreg16(rxintr, MUSB_INTRRX);
    }

  /* Filter out disabled interrupts */

  usbintr &= musb_getreg32(MUSB_INTRUSBE);
  txintr  &= musb_getreg16(MUSB_INTRTXE);
  rxintr  &= musb_getreg16(MUSB_INTRRXE);

  /* Handle bus reset (highest priority) */

  if (usbintr & MUSB_INTR_RESET)
    {
      t113_musb_reset(priv);
      g_usb_reset_count++;
      return OK;
    }

  /* Handle suspend */

  if (usbintr & MUSB_INTR_SUSPEND)
    {
      usb_trace_info("USB suspend\n");
      priv->suspended = true;
      if (priv->driver != NULL)
        {
          CLASS_SUSPEND(priv->driver, &priv->usbdev);
        }
    }

  /* Handle resume */

  if (usbintr & MUSB_INTR_RESUME)
    {
      usb_trace_info("USB resume\n");
      priv->suspended = false;
      if (priv->driver != NULL)
        {
          CLASS_RESUME(priv->driver, &priv->usbdev);
        }
    }

  /* Handle EP0 (TX interrupt bit 0) */

  t113_ep_select(0);
  {
    uint16_t csr0 = musb_getreg16(MUSB_CSR0);
    g_usb_ep0_csr0 = csr0;
    if ((txintr & 1) || (csr0 & MUSB_CSR0_RXPKTRDY))
      {
        t113_ep0_setup(priv);
        g_usb_ep0_count++;
        g_usb_ep0_state = priv->ep0state;
      }
  }

  /* Handle EPn TX complete (bits 1-4) */

  for (i = 1; i < T113_NPHYSEP; i++)
    {
      if (txintr & (1 << i))
        {
          t113_epn_txdone(priv, i);
        }
    }

  /* Handle EPn RX ready (bits 1-4) */

  for (i = 1; i < T113_NPHYSEP; i++)
    {
      if (rxintr & (1 << i))
        {
          t113_epn_rxready(priv, i);
        }
    }

  /* Restore EP index */

  musb_putreg8(old_index, MUSB_INDEX);

  return OK;
}

/****************************************************************************
 * Name: t113_ccu_init
 *
 * Description:
 *   Initialize USB clocks and deassert resets.
 *
 ****************************************************************************/

static void t113_ccu_init(void)
{
  uint32_t reg;

  reg = getreg32(T113_CCU_USB0_CLK);
  reg |= USB0_CLK_PHYRST_DEASSERT;
  putreg32(reg, T113_CCU_USB0_CLK);

  reg = getreg32(T113_CCU_USB_BGR);
  reg |= USB_BGR_OTG0_RST | USB_BGR_OTG0_GATING;
  putreg32(reg, T113_CCU_USB_BGR);

  up_mdelay(2);
}

/****************************************************************************
 * Name: t113_phy_init
 *
 * Description:
 *   Initialize the USB PHY for device mode.
 *
 ****************************************************************************/

static void t113_phy_init(void)
{
  uint32_t reg;

  /* Clear change detect bits first */

  phy_clrbits32(USBPHY_ISCR, USB_ISCR_VBUS_CHANGE_DETECT |
                              USB_ISCR_ID_CHANGE_DETECT |
                              USB_ISCR_DPDM_CHANGE_DETECT);

  /* Force ID high (device mode) */

  reg = phy_getreg32(USBPHY_ISCR);
  reg &= ~USB_ISCR_FORCE_ID_MASK;
  reg |= USB_ISCR_FORCE_ID_HIGH;
  phy_putreg32(reg, USBPHY_ISCR);

  /* Force VBUS valid (always assume connected) */

  reg = phy_getreg32(USBPHY_ISCR);
  reg &= ~USB_ISCR_FORCE_VBUS_MASK;
  reg |= USB_ISCR_FORCE_VBUS_HIGH;
  phy_putreg32(reg, USBPHY_ISCR);

  /* Clear change detect again */

  phy_clrbits32(USBPHY_ISCR, USB_ISCR_VBUS_CHANGE_DETECT |
                              USB_ISCR_ID_CHANGE_DETECT |
                              USB_ISCR_DPDM_CHANGE_DETECT);

  /* 28nm PHY: Clear SIDDQ to exit power-down */

  phy_clrbits32(USBPHY_PHYCTL28NM, USB_PHYCTL28NM_SIDDQ);

  /* Select OTG mode */

  phy_setbits32(USBPHY_PHYSEL, USB_PHYSEL_OTG_SEL);

  /* Select PIO bus mode (VEND0 register) */

  musb_putreg8(0, MUSB_VEND0);

  /* PHY calibration via VC bus (bit-bang through PHYCTL28NM register).
   * Without this calibration, MUSB POWER.SOFTCONN cannot be written.
   */

  {
    uint32_t phyctl;
    int j;

    /* Write 0xC = 0x01 (enable calibration, 1 bit) */

    static const struct
    {
      uint8_t addr;
      uint8_t data;
      uint8_t len;
    } vc_seq[] =
    {
      { 0x0c, 0x01, 1 },
      { 0x20, 0x03, 2 },
      { 0x03, 0x00, 2 },
    };

    int s;

    for (s = 0; s < 3; s++)
      {
        uint8_t dtmp = vc_seq[s].data;

        phyctl = phy_getreg32(USBPHY_PHYCTL28NM);
        phyctl |= (1 << 1);
        phy_putreg32(phyctl, USBPHY_PHYCTL28NM);

        for (j = 0; j < vc_seq[s].len; j++)
          {
            phyctl = phy_getreg32(USBPHY_PHYCTL28NM);
            phyctl &= ~(1 << 0);
            phy_putreg32(phyctl, USBPHY_PHYCTL28NM);

            phyctl = phy_getreg32(USBPHY_PHYCTL28NM);
            phyctl &= ~(0xff << 8);
            phyctl |= ((vc_seq[s].addr + j) << 8);
            phy_putreg32(phyctl, USBPHY_PHYCTL28NM);

            phyctl = phy_getreg32(USBPHY_PHYCTL28NM);
            phyctl &= ~(1 << 7);
            phyctl |= ((dtmp & 0x01) << 7);
            phy_putreg32(phyctl, USBPHY_PHYCTL28NM);

            phyctl |= (1 << 0);
            phy_putreg32(phyctl, USBPHY_PHYCTL28NM);

            phyctl &= ~(1 << 0);
            phy_putreg32(phyctl, USBPHY_PHYCTL28NM);

            dtmp >>= 1;
          }

        phyctl = phy_getreg32(USBPHY_PHYCTL28NM);
        phyctl &= ~(1 << 1);
        phy_putreg32(phyctl, USBPHY_PHYCTL28NM);
      }
  }

  phy_putreg32(USB_PHYCTL28NM_VBUSVLDEXT, USBPHY_PHYCTL28NM);

  up_mdelay(1);
}

/****************************************************************************
 * Name: t113_musb_init
 *
 * Description:
 *   Initialize the MUSB controller registers.
 *
 ****************************************************************************/

static void t113_musb_init(struct t113_usbdev_s *priv)
{
  int i;

  musb_putreg32(0, MUSB_INTRUSBE);
  musb_putreg16(0, MUSB_INTRTXE);
  musb_putreg16(0, MUSB_INTRRXE);
  musb_putreg16(0xffff, MUSB_INTRTX);
  musb_putreg16(0xffff, MUSB_INTRRX);
  musb_putreg32(0xff, MUSB_INTRUSB);
  musb_clrbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);

  musb_putreg8(0, MUSB_FADDR);

  t113_ep_select(0);
  {
    uint16_t cnt = musb_getreg16(MUSB_RXCOUNT);
    while (cnt > 0)
      {
        (void)musb_getreg8(MUSB_FIFO(0));
        cnt--;
      }

    musb_putreg16(MUSB_CSR0_FLUSHFIFO, MUSB_CSR0);
    musb_putreg16(MUSB_CSR0_SVDSETUPEND | MUSB_CSR0_SVDRXPKTRDY,
                  MUSB_CSR0);
  }

  for (i = 0; i < (int)NFIFOCONFIGS; i++)
    {
      const struct t113_fifoconfig_s *cfg = &g_fifoconfig[i];
      uint8_t fifosz;

      t113_ep_select(cfg->epno);

      if (cfg->epno == 0)
        {
          musb_putreg16(MUSB_CSR0_FLUSHFIFO, MUSB_CSR0);
          continue;
        }

      fifosz = cfg->fifosz;
      if (cfg->dpb)
        {
          fifosz |= 0x10;
        }

      if (cfg->is_in)
        {
          musb_putreg16(MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_CLRDATATOG,
                        MUSB_TXCSR);
          musb_putreg16(MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_CLRDATATOG,
                        MUSB_TXCSR);
          musb_putreg16(cfg->size, MUSB_TXMAXP);
          musb_putreg8(fifosz, MUSB_TXFIFOSZ);
          musb_putreg16(FIFO_ADDR(cfg->addr), MUSB_TXFIFOADD);
        }
      else
        {
          musb_putreg16(MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_CLRDATATOG,
                        MUSB_RXCSR);
          musb_putreg16(MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_CLRDATATOG,
                        MUSB_RXCSR);
          musb_putreg16(cfg->size, MUSB_RXMAXP);
          musb_putreg8(fifosz, MUSB_RXFIFOSZ);
          musb_putreg16(FIFO_ADDR(cfg->addr), MUSB_RXFIFOADD);
        }
    }

  t113_ep_select(0);

  priv->ep0state = EP0STATE_IDLE;
  priv->attached = true;
}

static void t113_musb_enable(void)
{
  /* usbc_udc_enable: configure and set SOFTCONN */

  musb_clrbits8(MUSB_POWER, MUSB_POWER_ISOUPDATE);

  musb_putreg32(MUSB_INTR_SUSPEND | MUSB_INTR_RESUME | MUSB_INTR_RESET,
               MUSB_INTRUSBE);
  musb_putreg16(1, MUSB_INTRTXE);

  musb_setbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);
}

/****************************************************************************
 * Endpoint Operations
 ****************************************************************************/

/****************************************************************************
 * Name: t113_epconfigure
 *
 * Description:
 *   Configure an endpoint with the given descriptor.
 *
 ****************************************************************************/

static int t113_epconfigure(struct usbdev_ep_s *ep,
                            const struct usb_epdesc_s *desc,
                            bool last)
{
  struct t113_ep_s *privep = (struct t113_ep_s *)ep;
  uint16_t maxpacket;
  uint8_t  epno;
  bool     is_in;
  irqstate_t flags;

  DEBUGASSERT(ep != NULL && desc != NULL);

  epno = USB_EPNO(desc->addr);
  is_in = USB_ISEPIN(desc->addr);
  maxpacket = GETUINT16(desc->mxpacketsize);

  usb_trace_info("EP%d %s configure: maxpkt=%d type=%d\n",
                 epno, is_in ? "IN" : "OUT", maxpacket,
                 desc->attr & USB_EP_ATTR_XFERTYPE_MASK);

  flags = up_irq_save();

  ep->maxpacket = maxpacket;
  privep->eptype = desc->attr & USB_EP_ATTR_XFERTYPE_MASK;
  privep->stalled = false;

  /* Configure the hardware endpoint */

  t113_ep_select(epno);

  if (is_in)
    {
      /* Set TX maxpacket */

      musb_putreg16(maxpacket, MUSB_TXMAXP);

      /* Clear and flush TX FIFO */

      musb_putreg16(MUSB_TXCSR_MODE | MUSB_TXCSR_CLRDATATOG |
                    MUSB_TXCSR_FLUSHFIFO, MUSB_TXCSR);
      musb_putreg16(MUSB_TXCSR_MODE | MUSB_TXCSR_CLRDATATOG |
                    MUSB_TXCSR_FLUSHFIFO, MUSB_TXCSR);

      /* Enable TX interrupt for this EP */

      musb_setbits16(MUSB_INTRTXE, (1 << epno));
    }
  else
    {
      /* Set RX maxpacket */

      musb_putreg16(maxpacket, MUSB_RXMAXP);

      /* Clear and flush RX FIFO */

      musb_putreg16(MUSB_RXCSR_CLRDATATOG | MUSB_RXCSR_FLUSHFIFO,
                    MUSB_RXCSR);
      musb_putreg16(MUSB_RXCSR_CLRDATATOG | MUSB_RXCSR_FLUSHFIFO,
                    MUSB_RXCSR);

      /* Enable RX interrupt for this EP */

      musb_setbits16(MUSB_INTRRXE, (1 << epno));
    }

  up_irq_restore(flags);
  return OK;
}

/****************************************************************************
 * Name: t113_epdisable
 *
 * Description:
 *   Disable an endpoint.
 *
 ****************************************************************************/

static int t113_epdisable(struct usbdev_ep_s *ep)
{
  struct t113_ep_s *privep = (struct t113_ep_s *)ep;
  irqstate_t flags;

  DEBUGASSERT(ep != NULL);

  usb_trace_info("EP%d %s disable\n",
                 privep->epphy, privep->is_in ? "IN" : "OUT");

  flags = up_irq_save();

  privep->stalled = true;

  /* Disable endpoint interrupt */

  t113_ep_select(privep->epphy);

  if (privep->is_in)
    {
      musb_clrbits16(MUSB_INTRTXE, (1 << privep->epphy));
      musb_putreg16(MUSB_TXCSR_FLUSHFIFO, MUSB_TXCSR);
      musb_putreg16(MUSB_TXCSR_FLUSHFIFO, MUSB_TXCSR);
    }
  else
    {
      musb_clrbits16(MUSB_INTRRXE, (1 << privep->epphy));
      musb_putreg16(MUSB_RXCSR_FLUSHFIFO, MUSB_RXCSR);
      musb_putreg16(MUSB_RXCSR_FLUSHFIFO, MUSB_RXCSR);
    }

  /* Cancel all pending requests */

  t113_cancelrequests(privep, -ESHUTDOWN);

  up_irq_restore(flags);
  return OK;
}

/****************************************************************************
 * Name: t113_epallocreq
 *
 * Description:
 *   Allocate a USB request.
 *
 ****************************************************************************/

static struct usbdev_req_s *t113_epallocreq(struct usbdev_ep_s *ep)
{
  struct t113_req_s *privreq;

  DEBUGASSERT(ep != NULL);

  privreq = kmm_zalloc(sizeof(struct t113_req_s));
  if (privreq == NULL)
    {
      usb_trace_err("epallocreq: out of memory\n");
      return NULL;
    }

  return &privreq->req;
}

/****************************************************************************
 * Name: t113_epfreereq
 *
 * Description:
 *   Free a USB request.
 *
 ****************************************************************************/

static void t113_epfreereq(struct usbdev_ep_s *ep,
                           struct usbdev_req_s *req)
{
  struct t113_req_s *privreq = (struct t113_req_s *)req;

  DEBUGASSERT(ep != NULL && req != NULL);
  kmm_free(privreq);
}

/****************************************************************************
 * Name: t113_epsubmit
 *
 * Description:
 *   Submit a USB request for transfer.
 *
 ****************************************************************************/

static int t113_epsubmit(struct usbdev_ep_s *ep,
                         struct usbdev_req_s *req)
{
  struct t113_ep_s  *privep  = (struct t113_ep_s *)ep;
  struct t113_req_s *privreq = (struct t113_req_s *)req;
  struct t113_usbdev_s *priv;
  irqstate_t flags;
  bool was_empty;

  DEBUGASSERT(ep != NULL && req != NULL && req->callback != NULL &&
              req->buf != NULL);

  priv = privep->dev;

  req->result = -EINPROGRESS;
  req->xfrd   = 0;
  g_usb_ep0_submit++;

  flags = up_irq_save();

  /* Handle EP0 specially */

  if (privep->epphy == 0)
    {
      /* EP0 IN or OUT based on current state */

      if (privep->is_in)
        {
          /* EP0 IN: send data */

          t113_rqenqueue(privep, privreq);

          if (priv->ep0state == EP0STATE_SETUP_IN ||
              priv->ep0state == EP0STATE_DATA_IN)
            {
              uint16_t xfrlen = req->len;
              g_usb_ep0_submit++;

              if (xfrlen > EP0_MAXPACKET)
                {
                  xfrlen = EP0_MAXPACKET;
                }

              if (xfrlen > priv->ep0reqlen)
                {
                  xfrlen = priv->ep0reqlen;
                }

              t113_ep_select(0);
              t113_fifo_write(0, req->buf, xfrlen);
              req->xfrd = xfrlen;
              priv->ep0reqlen -= xfrlen;

              if (xfrlen < EP0_MAXPACKET || req->xfrd >= req->len ||
                  priv->ep0reqlen == 0)
                {
                  musb_putreg16(MUSB_CSR0_TXPKTRDY |
                                MUSB_CSR0_DATAEND, MUSB_CSR0);
                  g_usb_ep0_txpktrdy++;
                  g_usb_ep0_csr0 = musb_getreg16(MUSB_CSR0);
                  priv->ep0state = EP0STATE_WAIT_STATUS_OUT;

                  privreq = t113_rqdequeue(privep);
                  if (privreq != NULL)
                    {
                      t113_reqcomplete(privep, privreq, OK);
                    }
                }
              else
                {
                  musb_putreg16(MUSB_CSR0_TXPKTRDY, MUSB_CSR0);
                  priv->ep0state = EP0STATE_DATA_IN;
                }
            }
        }
      else
        {
          /* EP0 OUT: receive data */

          t113_rqenqueue(privep, privreq);
        }

      up_irq_restore(flags);
      return OK;
    }

  /* EPn: add to queue */

  if (privep->stalled)
    {
      up_irq_restore(flags);
      return -EPERM;
    }

  was_empty = t113_rqenqueue(privep, privreq);

  if (was_empty)
    {
      if (privep->is_in)
        {
          /* Start TX transfer */

          t113_epn_txstart(privep);
        }
      else
        {
          /* Check if there's already data waiting in FIFO */

          t113_ep_select(privep->epphy);
          if (musb_getreg16(MUSB_RXCSR) & MUSB_RXCSR_RXPKTRDY)
            {
              t113_epn_rxready(priv, privep->epphy);
            }
        }
    }

  up_irq_restore(flags);
  return OK;
}

/****************************************************************************
 * Name: t113_epcancel
 *
 * Description:
 *   Cancel a pending USB request.
 *
 ****************************************************************************/

static int t113_epcancel(struct usbdev_ep_s *ep,
                         struct usbdev_req_s *req)
{
  struct t113_ep_s *privep = (struct t113_ep_s *)ep;
  irqstate_t flags;

  DEBUGASSERT(ep != NULL && req != NULL);

  flags = up_irq_save();
  t113_cancelrequests(privep, -ECONNRESET);
  up_irq_restore(flags);

  return OK;
}

/****************************************************************************
 * Name: t113_epstall
 *
 * Description:
 *   Stall or resume an endpoint.
 *
 ****************************************************************************/

static int t113_epstall(struct usbdev_ep_s *ep, bool resume)
{
  struct t113_ep_s *privep = (struct t113_ep_s *)ep;
  irqstate_t flags;

  DEBUGASSERT(ep != NULL);

  usb_trace_info("EP%d %s %s\n", privep->epphy,
                 privep->is_in ? "IN" : "OUT",
                 resume ? "resume" : "stall");

  flags = up_irq_save();

  t113_ep_select(privep->epphy);

  if (privep->epphy == 0)
    {
      /* EP0 stall */

      if (!resume)
        {
          privep->stalled = true;
          musb_putreg16(MUSB_CSR0_SENDSTALL |
                        MUSB_CSR0_SVDRXPKTRDY, MUSB_CSR0);
          privep->dev->ep0state = EP0STATE_STALL;
        }
      else
        {
          privep->stalled = false;
          privep->dev->ep0state = EP0STATE_IDLE;
        }
    }
  else if (privep->is_in)
    {
      if (resume)
        {
          /* Clear stall */

          privep->stalled = false;
          musb_clrbits16(MUSB_TXCSR, MUSB_TXCSR_SENDSTALL |
                                      MUSB_TXCSR_SENTSTALL);
          musb_setbits16(MUSB_TXCSR, MUSB_TXCSR_CLRDATATOG);
        }
      else
        {
          privep->stalled = true;
          musb_setbits16(MUSB_TXCSR, MUSB_TXCSR_SENDSTALL);
        }
    }
  else
    {
      if (resume)
        {
          /* Clear stall */

          privep->stalled = false;
          musb_clrbits16(MUSB_RXCSR, MUSB_RXCSR_SENDSTALL |
                                      MUSB_RXCSR_SENTSTALL);
          musb_setbits16(MUSB_RXCSR, MUSB_RXCSR_CLRDATATOG);
        }
      else
        {
          privep->stalled = true;
          musb_setbits16(MUSB_RXCSR, MUSB_RXCSR_SENDSTALL);
        }
    }

  up_irq_restore(flags);
  return OK;
}

/****************************************************************************
 * Device Operations
 ****************************************************************************/

/****************************************************************************
 * Name: t113_allocep
 *
 * Description:
 *   Allocate an endpoint.
 *
 ****************************************************************************/

static struct usbdev_ep_s *t113_allocep(struct usbdev_s *dev,
                                        uint8_t epphy, bool in,
                                        uint8_t eptype)
{
  struct t113_usbdev_s *priv = (struct t113_usbdev_s *)dev;
  struct t113_ep_s *privep;
  irqstate_t flags;
  int idx;
  int epno;

  DEBUGASSERT(dev != NULL);

  epphy &= USB_EPNO_MASK;

  flags = up_irq_save();

  if (epphy == 0)
    {
      for (epno = 1; epno < T113_NPHYSEP; epno++)
        {
          idx = in ? T113_EPPHYIN(epno) : T113_EPPHYOUT(epno);
          if ((priv->epavail & (1 << idx)) != 0)
            {
              priv->epavail &= ~(1 << idx);
              privep = &priv->eplist[idx];
              up_irq_restore(flags);

              usb_trace_info("allocep: EP%d %s\n",
                             epno, in ? "IN" : "OUT");
              return &privep->ep;
            }
        }
    }
  else
    {
      /* Allocate the specific EP */

      epno = epphy;
      if (epno < T113_NPHYSEP)
        {
          idx = in ? T113_EPPHYIN(epno) : T113_EPPHYOUT(epno);
          if ((priv->epavail & (1 << idx)) != 0)
            {
              priv->epavail &= ~(1 << idx);
              privep = &priv->eplist[idx];
              up_irq_restore(flags);

              usb_trace_info("allocep: EP%d %s (specific)\n",
                             epno, in ? "IN" : "OUT");
              return &privep->ep;
            }
        }
    }

  up_irq_restore(flags);
  usb_trace_err("allocep: no EP available\n");
  return NULL;
}

/****************************************************************************
 * Name: t113_freeep
 *
 * Description:
 *   Free an endpoint.
 *
 ****************************************************************************/

static void t113_freeep(struct usbdev_s *dev, struct usbdev_ep_s *ep)
{
  struct t113_usbdev_s *priv = (struct t113_usbdev_s *)dev;
  struct t113_ep_s *privep = (struct t113_ep_s *)ep;
  irqstate_t flags;
  int idx;

  DEBUGASSERT(dev != NULL && ep != NULL);

  idx = privep->is_in ? T113_EPPHYIN(privep->epphy) :
                         T113_EPPHYOUT(privep->epphy);

  flags = up_irq_save();
  priv->epavail |= (1 << idx);
  up_irq_restore(flags);

  usb_trace_info("freeep: EP%d %s\n",
                 privep->epphy, privep->is_in ? "IN" : "OUT");
}

/****************************************************************************
 * Name: t113_getframe
 *
 * Description:
 *   Get the current frame number.
 *
 ****************************************************************************/

static int t113_getframe(struct usbdev_s *dev)
{
  UNUSED(dev);
  return (int)musb_getreg16(MUSB_FRAME);
}

/****************************************************************************
 * Name: t113_wakeup
 *
 * Description:
 *   Send remote wakeup signal.
 *
 ****************************************************************************/

static int t113_wakeup(struct usbdev_s *dev)
{
  UNUSED(dev);
  musb_setbits8(MUSB_POWER, MUSB_POWER_RESUME);
  up_mdelay(10);
  musb_clrbits8(MUSB_POWER, MUSB_POWER_RESUME);
  return OK;
}

/****************************************************************************
 * Name: t113_selfpowered
 *
 * Description:
 *   Set the self-powered status.
 *
 ****************************************************************************/

static int t113_selfpowered(struct usbdev_s *dev, bool selfpowered)
{
  struct t113_usbdev_s *priv = (struct t113_usbdev_s *)dev;
  priv->selfpowered = selfpowered;
  return OK;
}

/****************************************************************************
 * Name: t113_pullup
 *
 * Description:
 *   Control the USB pull-up resistor (connect/disconnect from host).
 *
 ****************************************************************************/

static int t113_pullup(struct usbdev_s *dev, bool enable)
{
  UNUSED(dev);

  if (enable)
    {
      musb_setbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);
    }
  else
    {
      musb_clrbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_usbinitialize
 *
 * Description:
 *   Initialize USB hardware (CCU, PHY, MUSB).
 *   Must be called from arm_boot (early init) because T113 MUSB
 *   POWER.SOFTCONN can only be written before scheduler starts.
 *
 ****************************************************************************/

void arm_usbinitialize(void)
{
  struct t113_usbdev_s *priv = &g_usbdev;
  int i;

  usb_trace_info("T113 MUSB USB device initialization\n");

  /* Zero out the device state */

  memset(priv, 0, sizeof(struct t113_usbdev_s));

  /* Set up the standard device structure */

  priv->usbdev.ops = &g_devops;
  priv->usbdev.ep0 = &priv->eplist[T113_EP0_IN].ep;
  priv->usbdev.speed = USB_SPEED_FULL;
  priv->usbdev.dualspeed = 0;

  /* Initialize all endpoint structures */

  for (i = 0; i < T113_NLOGEP; i++)
    {
      struct t113_ep_s *privep = &priv->eplist[i];

      privep->ep.ops = &g_epops;
      privep->dev = priv;
      privep->head = NULL;
      privep->tail = NULL;
      privep->stalled = false;

      /* Determine physical EP number and direction */

      privep->epphy = i / 2;
      privep->is_in = (i % 2) == 0;

      if (privep->epphy == 0)
        {
          /* EP0 */

          privep->ep.eplog = 0;
          privep->ep.maxpacket = EP0_MAXPACKET;
        }
      else
        {
          /* EPn */

          privep->ep.eplog = privep->is_in ?
                             PHYIN2LOG(privep->epphy) :
                             PHYOUT2LOG(privep->epphy);

          /* Set default maxpacket from FIFO config */

          if (privep->epphy <= 2)
            {
              privep->ep.maxpacket = 1024;
            }
          else
            {
              privep->ep.maxpacket = 512;
            }
        }

      /* FIFO size lookup */

      privep->fifosz = privep->ep.maxpacket;
    }

  /* Available endpoints (all except EP0) */

  priv->epavail = T113_EPALLSET & ~T113_EPCTRLSET;

  t113_ccu_init();
  t113_phy_init();
  t113_musb_init(priv);

  irq_attach(T113_IRQ_USB0_DEVICE, t113_usbdev_interrupt, priv);
  up_enable_irq(T113_IRQ_USB0_DEVICE);

#ifdef CONFIG_CDCACM
  {
    extern int cdcacm_initialize(int minor, FAR void **handle);
    cdcacm_initialize(0, NULL);
  }
#endif

  t113_musb_enable();
}

/****************************************************************************
 * Name: arm_usbuninitialize
 *
 * Description:
 *   Uninitialize USB device hardware.
 *
 ****************************************************************************/

void arm_usbuninitialize(void)
{
  struct t113_usbdev_s *priv = &g_usbdev;

  /* Disable interrupts */

  up_disable_irq(T113_IRQ_USB0_DEVICE);

  /* Disconnect from host */

  musb_clrbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);

  /* Disable all interrupt sources */

  musb_putreg32(0, MUSB_INTRUSBE);
  musb_putreg16(0, MUSB_INTRTXE);
  musb_putreg16(0, MUSB_INTRRXE);

  /* Detach interrupt handler */

  irq_detach(T113_IRQ_USB0_DEVICE);

  /* Cancel all pending requests */

  for (int i = 0; i < T113_NLOGEP; i++)
    {
      t113_cancelrequests(&priv->eplist[i], -ESHUTDOWN);
    }
}

/****************************************************************************
 * Name: usbdev_register
 *
 * Description:
 *   Register a USB device class driver.  The class driver's bind() method
 *   will be called to bind it to a USB device driver.
 *
 ****************************************************************************/

int usbdev_register(struct usbdevclass_driver_s *driver)
{
  struct t113_usbdev_s *priv = &g_usbdev;
  int ret;

  DEBUGASSERT(driver != NULL && driver->ops->bind != NULL &&
              driver->ops->unbind != NULL &&
              driver->ops->setup != NULL &&
              driver->ops->disconnect != NULL);

  if (priv->driver != NULL)
    {
      usb_trace_err("usbdev_register: already bound\n");
      return -EBUSY;
    }

  /* Bind the class driver */

  priv->driver = driver;
  ret = CLASS_BIND(driver, &priv->usbdev);
  if (ret < 0)
    {
      usb_trace_err("usbdev_register: bind failed: %d\n", ret);
      priv->driver = NULL;
      return ret;
    }

  usb_trace_info("usbdev_register: class driver bound\n");

  /* Force USB re-enumeration: disconnect then reconnect.
   * Must use putreg32 because 8-bit writes to MUSB_POWER
   * are ignored in task context on T113.
   */

  {
    uint32_t pwr = musb_getreg32(MUSB_POWER & ~3u);
    pwr &= ~(uint32_t)MUSB_POWER_SOFTCONN;
    musb_putreg32(pwr, MUSB_POWER & ~3u);
    up_mdelay(500);
    pwr |= (uint32_t)MUSB_POWER_SOFTCONN;
    musb_putreg32(pwr, MUSB_POWER & ~3u);
  }

  return OK;
}

/****************************************************************************
 * Name: usbdev_unregister
 *
 * Description:
 *   Unregister a USB device class driver.
 *
 ****************************************************************************/

int usbdev_unregister(struct usbdevclass_driver_s *driver)
{
  struct t113_usbdev_s *priv = &g_usbdev;

  DEBUGASSERT(driver != NULL);

  if (priv->driver != driver)
    {
      return -EINVAL;
    }

  /* Disconnect from host */

  musb_clrbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);

  /* Unbind the class driver */

  CLASS_DISCONNECT(driver, &priv->usbdev);
  CLASS_UNBIND(driver, &priv->usbdev);

  priv->driver = NULL;

  /* Reconnect to allow new class driver binding */

  musb_setbits8(MUSB_POWER, MUSB_POWER_SOFTCONN);

  usb_trace_info("usbdev_unregister: class driver unbound\n");
  return OK;
}
