/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_serial.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) SHUB_UART 专用串口驱动
 *
 * RCPU UART 为 PXA (Intel XScale) 变体：ns16550 兼容 + PXA 扩展位。
 * 标准 ns16550 驱动（uart_16550.c）不适配 PXA，差异点（K1 手册 16.3）：
 *   1) RDA（数据可用）中断在硬件上依赖 IER.RTOIE（仅使能 RAVIE 时 RX
 *      中断不上报，GDB 实测 IIR 恒 NO_INT）；
 *   2) IIR 编码：0xC6=RLS（最高优先级）、0xC4=RDA、0xC2=TX；RTO 用
 *      IIR.TOD(bit3)，ns16550 不处理此位（会中断风暴）；
 *   3) RTO（接收超时）处理流程：禁 RTOIE → 读 FIFO → 重使能 RTOIE；
 *   4) 32 位外设总线需设 FCR.BUS；TX 中断为 FIFO 半空触发（FCR.TIL=0）。
 *
 * 本驱动只实现程序 I/O（非 DMA），中断驱动，注册为 /dev/ttyS0 + console。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/serial/serial.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/k1r_memorymap.h"
#include "hardware/k1r_uart.h"

#ifdef CONFIG_K1R_UART

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define K1R_UART_BAUD          CONFIG_K1R_UART0_BAUD
#define K1R_UART_CLOCK         CONFIG_K1R_UART0_CLOCK
#define K1R_UART_IRQ           K1R_IRQ_UART0
#define K1R_UART_BASE          K1R_UART0_BASE

/* 波特率分频 = clk / (16 * baud)，取整 */
#define K1R_UART_DIV           ((K1R_UART_CLOCK) / (16 * (K1R_UART_BAUD)))

#ifdef CONFIG_K1R_UART0_SERIAL_CONSOLE
#  define CONSOLE_DEV          g_uart0dev
#  define TTYS0_DEV            g_uart0dev
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct k1r_pxa_s
{
  uintptr_t  base;              /* UART 寄存器基址 */
  int        irq;               /* ECLIC 中断号 */
  uint32_t   ier;               /* 维护的 IER 缓存 */
};

static struct k1r_pxa_s g_uart0priv =
{
  .base = K1R_UART_BASE,
  .irq  = K1R_UART_IRQ,
  .ier  = K1R_UART_IER_INTMODE,
};

/* 环形收发缓冲（NuttX 串口框架使用） */
#ifdef CONFIG_K1R_UART0_RXBUFSIZE
#  define K1R_RXBUFSIZE CONFIG_K1R_UART0_RXBUFSIZE
#else
#  define K1R_RXBUFSIZE 256
#endif
#ifdef CONFIG_K1R_UART0_TXBUFSIZE
#  define K1R_TXBUFSIZE CONFIG_K1R_UART0_TXBUFSIZE
#else
#  define K1R_TXBUFSIZE 256
#endif

static char g_uart0_rxbuf[K1R_RXBUFSIZE];
static char g_uart0_txbuf[K1R_TXBUFSIZE];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void     k1r_uart_putreg(struct k1r_pxa_s *priv, uint32_t off,
                                uint32_t value);
static uint32_t k1r_uart_getreg(struct k1r_pxa_s *priv, uint32_t off);
static int      k1r_uart_setup(FAR struct uart_dev_s *dev);
static void     k1r_uart_shutdown(FAR struct uart_dev_s *dev);
static int      k1r_uart_attach(FAR struct uart_dev_s *dev);
static void     k1r_uart_detach(FAR struct uart_dev_s *dev);
static int      k1r_uart_ioctl(FAR struct file *filep, int cmd,
                               unsigned long arg);
static int      k1r_uart_receive(FAR struct uart_dev_s *dev,
                                 FAR unsigned int *status);
static void     k1r_uart_rxint(FAR struct uart_dev_s *dev, bool enable);
static bool     k1r_uart_rxavailable(FAR struct uart_dev_s *dev);
static void     k1r_uart_send(FAR struct uart_dev_s *dev, int ch);
static void     k1r_uart_txint(FAR struct uart_dev_s *dev, bool enable);
static bool     k1r_uart_txready(FAR struct uart_dev_s *dev);
static bool     k1r_uart_txempty(FAR struct uart_dev_s *dev);
static int      k1r_uart_interrupt(int irq, void *context, void *arg);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_uart_putreg / k1r_uart_getreg
 ****************************************************************************/

static void k1r_uart_putreg(struct k1r_pxa_s *priv, uint32_t off,
                            uint32_t value)
{
  *(volatile uint32_t *)(priv->base + off) = value;
}

static uint32_t k1r_uart_getreg(struct k1r_pxa_s *priv, uint32_t off)
{
  return *(volatile uint32_t *)(priv->base + off) & 0xff;
}

/****************************************************************************
 * Name: k1r_uart_setup
 *
 * 配置波特率、8N1、FIFO/FCR、IER（PXA 中断模式）。不使能中断（由
 * rxint/txint 控制）。
 ****************************************************************************/

static int k1r_uart_setup(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;
  uint32_t lcr;

  if (priv->base == 0)
    {
      return -EPERM;
    }

  /* 清 FIFO（FCR：disable 后清） */
  k1r_uart_putreg(priv, K1R_UART_FCR_IIR, 0);

  /* 配置波特率：DLAB=1，写 DLL/DLM */
  lcr = K1R_UART_LCR_WLS_8 | K1R_UART_LCR_DLAB;
  k1r_uart_putreg(priv, K1R_UART_LCR, lcr);
  k1r_uart_putreg(priv, K1R_UART_RBR_THR_DLL, K1R_UART_DIV & 0xff);
  k1r_uart_putreg(priv, K1R_UART_IER_DLM, (K1R_UART_DIV >> 8) & 0xff);

  /* DLAB=0，8N1（无校验/1 停止位） */
  lcr = K1R_UART_LCR_WLS_8;
  k1r_uart_putreg(priv, K1R_UART_LCR, lcr);

  /* MCR：OUT2（发送/中断输出使能关键位） */
  k1r_uart_putreg(priv, K1R_UART_MCR, K1R_UART_MCR_OUT2);

  /* FCR：双 FIFO 使能 + 32 位总线 + 1 字节 RX 触发阈值 + TX 半空中断 */
  k1r_uart_putreg(priv, K1R_UART_FCR_IIR, K1R_UART_FCR_VAL);

  /* IER：PXA 中断模式（RDA+RLS+RTO+UUE），TX 中断由 txint 按需开启 */
  priv->ier = K1R_UART_IER_INTMODE;
  k1r_uart_putreg(priv, K1R_UART_IER_DLM, priv->ier);

  return OK;
}

/****************************************************************************
 * Name: k1r_uart_shutdown
 ****************************************************************************/

static void k1r_uart_shutdown(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;

  /* 关 UART 使能 */
  priv->ier = 0;
  k1r_uart_putreg(priv, K1R_UART_IER_DLM, 0);
}

/****************************************************************************
 * Name: k1r_uart_attach
 ****************************************************************************/

static int k1r_uart_attach(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;
  int ret;

  ret = irq_attach(priv->irq, k1r_uart_interrupt, dev);
  if (ret < 0)
    {
      return ret;
    }

  up_enable_irq(priv->irq);

  return OK;
}

/****************************************************************************
 * Name: k1r_uart_detach
 ****************************************************************************/

static void k1r_uart_detach(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;

  up_disable_irq(priv->irq);
  irq_detach(priv->irq);
}

/****************************************************************************
 * Name: k1r_uart_ioctl
 ****************************************************************************/

static int k1r_uart_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Name: k1r_uart_receive
 ****************************************************************************/

static int k1r_uart_receive(FAR struct uart_dev_s *dev,
                            FAR unsigned int *status)
{
  struct k1r_pxa_s *priv = dev->priv;
  uint32_t lsr = k1r_uart_getreg(priv, K1R_UART_LSR);

  /* 返回 LSR 错误位；数据在 RBR */
  *status = lsr & (K1R_UART_LSR_OE | K1R_UART_LSR_PE |
                   K1R_UART_LSR_FE | K1R_UART_LSR_BI);

  return (int)(k1r_uart_getreg(priv, K1R_UART_RBR_THR_DLL) & 0xff);
}

/****************************************************************************
 * Name: k1r_uart_rxint
 ****************************************************************************/

static void k1r_uart_rxint(FAR struct uart_dev_s *dev, bool enable)
{
  struct k1r_pxa_s *priv = dev->priv;

  if (enable)
    {
      priv->ier |= K1R_UART_IER_RAVIE;
    }
  else
    {
      priv->ier &= ~K1R_UART_IER_RAVIE;
    }

  k1r_uart_putreg(priv, K1R_UART_IER_DLM, priv->ier);
}

/****************************************************************************
 * Name: k1r_uart_rxavailable
 ****************************************************************************/

static bool k1r_uart_rxavailable(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;

  return (k1r_uart_getreg(priv, K1R_UART_LSR) & K1R_UART_LSR_DR) != 0;
}

/****************************************************************************
 * Name: k1r_uart_send
 ****************************************************************************/

static void k1r_uart_send(FAR struct uart_dev_s *dev, int ch)
{
  struct k1r_pxa_s *priv = dev->priv;

  k1r_uart_putreg(priv, K1R_UART_RBR_THR_DLL, (uint32_t)ch & 0xff);
}

/****************************************************************************
 * Name: k1r_uart_txint
 ****************************************************************************/

static void k1r_uart_txint(FAR struct uart_dev_s *dev, bool enable)
{
  struct k1r_pxa_s *priv = dev->priv;

  if (enable)
    {
      priv->ier |= K1R_UART_IER_TIE;
    }
  else
    {
      priv->ier &= ~K1R_UART_IER_TIE;
    }

  k1r_uart_putreg(priv, K1R_UART_IER_DLM, priv->ier);
}

/****************************************************************************
 * Name: k1r_uart_txready
 *
 * PXA 的 THRE（Transmit Data Request）在 TX FIFO 半空时置位。
 ****************************************************************************/

static bool k1r_uart_txready(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;

  return (k1r_uart_getreg(priv, K1R_UART_LSR) & K1R_UART_LSR_THRE) != 0;
}

/****************************************************************************
 * Name: k1r_uart_txempty
 ****************************************************************************/

static bool k1r_uart_txempty(FAR struct uart_dev_s *dev)
{
  struct k1r_pxa_s *priv = dev->priv;

  return (k1r_uart_getreg(priv, K1R_UART_LSR) & K1R_UART_LSR_TEMT) != 0;
}

/****************************************************************************
 * Name: k1r_uart_interrupt
 *
 * 中断处理（PXA IIR 编码 + RTO 流程）。调用 uart_recvchars / uart_xmitchars。
 ****************************************************************************/

static int k1r_uart_interrupt(int irq, void *context, void *arg)
{
  FAR struct uart_dev_s *dev = (FAR struct uart_dev_s *)arg;
  struct k1r_pxa_s *priv = dev->priv;
  uint32_t iir;

  /* 读 IIR（读 FCR_IIR 地址）。bit0=NIP（0=有中断） */
  iir = k1r_uart_getreg(priv, K1R_UART_FCR_IIR);

  if (iir & K1R_UART_IIR_NIP)
    {
      /* 无 pending 中断 */
      return OK;
    }

  /* RTO（Time Out Detected，FIFO 有数据但 4 字符时间未处理）：
   * 手册 2079-2087：禁 RTOIE → 读 FIFO 直到空 → 重使能 RTOIE。 */
  if (iir & K1R_UART_IIR_TOD)
    {
      priv->ier &= ~K1R_UART_IER_RTOIE;
      k1r_uart_putreg(priv, K1R_UART_IER_DLM, priv->ier);

      while (k1r_uart_getreg(priv, K1R_UART_LSR) & K1R_UART_LSR_DR)
        {
          uart_recvchars(dev);
        }

      priv->ier |= K1R_UART_IER_RTOIE;
      k1r_uart_putreg(priv, K1R_UART_IER_DLM, priv->ier);

      return OK;
    }

  /* 按 IID10（bits[2:1]）分发 */
  switch (iir & K1R_UART_IIR_IID_MASK)
    {
      case K1R_UART_IIR_IID_RLS:  /* 接收线路错误（最高优先级） */
        {
          /* 读 LSR 清除错误标志 */
          (void)k1r_uart_getreg(priv, K1R_UART_LSR);
          break;
        }

      case K1R_UART_IIR_IID_RDA:  /* 接收数据可用 */
        {
          uart_recvchars(dev);
          break;
        }

      case K1R_UART_IIR_IID_TX:   /* 发送 FIFO 请求数据 */
        {
          uart_xmitchars(dev);
          break;
        }

      case K1R_UART_IIR_IID_MS:   /* Modem 状态 */
        {
          /* 读 MSR 清除 */
          (void)k1r_uart_getreg(priv, K1R_UART_MSR);
          break;
        }

      default:
        break;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_uart_register_common
 ****************************************************************************/

static const struct uart_ops_s g_uart_ops =
{
  .setup       = k1r_uart_setup,
  .shutdown    = k1r_uart_shutdown,
  .attach      = k1r_uart_attach,
  .detach      = k1r_uart_detach,
  .ioctl       = k1r_uart_ioctl,
  .receive     = k1r_uart_receive,
  .rxint       = k1r_uart_rxint,
  .rxavailable = k1r_uart_rxavailable,
  .send        = k1r_uart_send,
  .txint       = k1r_uart_txint,
  .txready     = k1r_uart_txready,
  .txempty     = k1r_uart_txempty,
};

static uart_dev_t g_uart0dev =
{
  .recv = {
    .size   = K1R_RXBUFSIZE,
    .buffer = g_uart0_rxbuf,
  },
  .xmit = {
    .size   = K1R_TXBUFSIZE,
    .buffer = g_uart0_txbuf,
  },
  .ops  = &g_uart_ops,
  .priv = &g_uart0priv,
};

/****************************************************************************
 * Name: riscv_earlyserialinit
 ****************************************************************************/

void riscv_earlyserialinit(void)
{
#ifdef CONSOLE_DEV
  CONSOLE_DEV.isconsole = true;
  k1r_uart_setup(&CONSOLE_DEV);
#endif
}

/****************************************************************************
 * Name: riscv_serialinit
 ****************************************************************************/

void riscv_serialinit(void)
{
#ifdef CONSOLE_DEV
  uart_register("/dev/console", &CONSOLE_DEV);
#endif
#ifdef TTYS0_DEV
  uart_register("/dev/ttyS0", &TTYS0_DEV);
#endif
}

/****************************************************************************
 * Name: up_putc
 *
 * 早期 syslog/debug 低层输出（VFS 之前）。阻塞等 THRE（FIFO 半空）后写 THR。
 ****************************************************************************/

void up_putc(int ch)
{
  struct k1r_pxa_s *priv = (struct k1r_pxa_s *)g_uart0dev.priv;

  while ((k1r_uart_getreg(priv, K1R_UART_LSR) & K1R_UART_LSR_THRE) == 0)
    {
    }

  k1r_uart_putreg(priv, K1R_UART_RBR_THR_DLL, (uint32_t)ch & 0xff);
}

#endif /* CONFIG_K1R_UART */
