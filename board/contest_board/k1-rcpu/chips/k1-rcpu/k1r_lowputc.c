/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_lowputc.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) 早期串口（PXA UART 变体）
 *
 * PXA UART 兼容 ns16550，但需设置 IER 的 UUE（Unit Enable）位
 * 才能工作（Zephyr PR #95248 的处理）。
 * UART0 @0xC0881000、UART1 @0xC088D000。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/arch.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/k1r_ccu.h"
#include "hardware/k1r_memorymap.h"
#include "hardware/k1r_uart.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_16550_UART0
#  define K1R_CONSOLE_BASE    CONFIG_16550_UART0_BASE
#else
#  define K1R_CONSOLE_BASE    K1R_UART0_BASE
#endif

#define K1R_UART_REG(base, off) \
  (*(volatile uint32_t *)((uintptr_t)(base) + (off)))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_uart_init
 ****************************************************************************/

static void k1r_uart_init(uintptr_t base)
{
  uint32_t lcr;

  /* DLAB=1 配置波特率（UART0 fclk=24.5MHz，115200：除数=24.5M/16/115200≈13） */

  lcr = K1R_UART_LCR_WLS_8 | K1R_UART_LCR_DLAB;
  K1R_UART_REG(base, K1R_UART_LCR) = lcr;
  K1R_UART_REG(base, K1R_UART_RBR_THR_DLL) = 13;
  K1R_UART_REG(base, K1R_UART_IER_DLM) = 0;

  /* DLAB=0，8N1 */

  K1R_UART_REG(base, K1R_UART_LCR) = K1R_UART_LCR_WLS_8;

  /* PXA UART 早期使能（showprogress 用，只做 TX）：
   * IER = UUE(0x40)：早期串口只输出进度字符，不使能 RX 中断；
   *   完整中断配置由 k1r_serial.c 的 k1r_uart_setup() 负责
   *   （IER=RDA|RLS|RTO|UUE、FCR=BUS32|ITL、MCR=OUT2）。
   * MCR = OUT2(0x08)   ← 关键：发送使能，漏掉则 THR 写不生效
   * FCR = 0x07         ← enable + clear RCVR + clear XMIT
   */

  K1R_UART_REG(base, K1R_UART_IER_DLM) = K1R_UART_IER_UUE;
  K1R_UART_REG(base, K1R_UART_MCR)     = K1R_UART_MCR_OUT2;
  K1R_UART_REG(base, K1R_UART_FCR_IIR) = 0x07;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_lowsetup
 ****************************************************************************/

void k1r_lowsetup(void)
{
  k1r_uart_init(K1R_CONSOLE_BASE);
}

/****************************************************************************
 * Name: riscv_lowputc
 ****************************************************************************/

void riscv_lowputc(char ch)
{
  uintptr_t base = K1R_CONSOLE_BASE;

  /* 等待发送 FIFO 空 */

  while ((K1R_UART_REG(base, K1R_UART_LSR) & K1R_UART_LSR_THRE) == 0);

  K1R_UART_REG(base, K1R_UART_RBR_THR_DLL) = (uint32_t)ch;
}

/****************************************************************************
 * Name: riscv_lowputs
 ****************************************************************************/

void riscv_lowputs(const char *str)
{
  while (*str)
    {
      riscv_lowputc(*str++);
    }
}
