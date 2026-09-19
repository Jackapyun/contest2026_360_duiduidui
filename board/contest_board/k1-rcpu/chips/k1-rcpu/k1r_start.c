/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_start.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) C 语言启动
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>

#include <nuttx/init.h>
#include <nuttx/arch.h>
#include <arch/board/board.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/k1r_ccu.h"
#include "hardware/k1r_memorymap.h"
#include "k1r_userspace.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_DEBUG_FEATURES
#  define showprogress(c) riscv_lowputc(c)
#else
#  define showprogress(c)
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_clear_bss
 ****************************************************************************/

void k1r_clear_bss(void)
{
  uint32_t *dest;

  for (dest = (uint32_t *)_sbss; dest < (uint32_t *)_ebss; )
    {
      *dest++ = 0;
    }
}

/****************************************************************************
 * Name: k1r_clockconfig
 *
 * Description:
 *   配置 RCPU 外设时钟/复位。
 *   注意：RCPU 上电与主时钟由 AP 侧控制（remoteproc 或 JTAG 场景下
 *   已由系统使能）。这里仅确保调试串口（UART0）时钟使能 + 复位释放。
 ****************************************************************************/

void k1r_clockconfig(void)
{
  volatile uint32_t *reg;

  /* 1. 使能 pll1_aud_24p5 分频时钟 gate（APBS APB_SPARE2_REG bit11）。
   *    UART0 fclk sel=0 → pll1_aud_24p5(24.576MHz)；该 gate 复位默认关，
   *    若不使能 fclk=0，UART 发送移位卡死（字符进 THR 但发不出去）。
   */

  *(volatile uint32_t *)K1R_APBS_SPARE2 |= K1R_PLL1_AUD_24P5_GATE;

  /* 2. UART0 时钟复位控制（0xC0880000 + 0xD8）：
   *    fclk sel=2 → vctcxo_24(24MHz 精确晶体振荡器，不依赖 PLL 锁定)。
   */

  reg = (volatile uint32_t *)(K1R_SYSCTRL_BASE +
                              K1R_SYSCTRL_UART0_CLK_RST);

  *reg = (K1R_CLK_FCLK_SEL_24M << K1R_CLK_FCLK_SEL_SHIFT) |
         (0 << K1R_CLK_FCLK_DIV_SHIFT) |
         K1R_CLK_PCLK_EN |
         K1R_CLK_FCLK_EN |
         K1R_CLK_SW_RSTN;

  /* 3. RCPU 核心时钟：内核自己配置，不依赖之前 AP/复位留下的状态。
   *    设置时钟源 = K1R_RCPU_CLK_SRC（默认 245.7 MHz = pll1_aud_245p7，N308 标称），
   *    分频 = K1R_RCPU_CLK_DIV（0 = div by 1）。
   *    读-改-写，保留 GATE(bit12)/FC(bit15) 等其它位；
   *    如需更高主频，把 K1R_RCPU_CLK_SRC 改成 _491P52(=2) 即可（需先确认 N308 稳定性）。
   */
  {
    volatile uint32_t *clk = (volatile uint32_t *)K1R_RCPU_CLK_RES_CTRL;
    uint32_t v = *clk;

    v &= ~((uint32_t)K1R_RCPU_CLK_MUX_MASK | (uint32_t)K1R_RCPU_CLK_DIV_MASK);
    v |= (uint32_t)((K1R_RCPU_CLK_SRC << K1R_RCPU_CLK_MUX_SHIFT) &
                    K1R_RCPU_CLK_MUX_MASK);
    v |= (uint32_t)((K1R_RCPU_CLK_DIV << K1R_RCPU_CLK_DIV_SHIFT) &
                    K1R_RCPU_CLK_DIV_MASK);

    *clk = v;
  }
}

/****************************************************************************
 * Name: __k1r_start
 ****************************************************************************/

void __k1r_start(void)
{
  /* Clear .bss */

  k1r_clear_bss();

  /* 注意：JTAG load 场景下 GDB 已按 VMA 把 .data 段写到 _sdata，无需再搬移。
   * 链接脚本 .data 的 LMA(_eronly) != VMA(_sdata)，是给 ROM/XIP 启动用的；
   * 若此处仍执行 _eronly→_sdata 搬移，会把正确的 .data 覆盖成 rodata 垃圾
   * （导致 g_uart0priv.ops 等函数指针失效，u16550_putc 崩溃）。
   */

  /* Configure clocks/resets for RCPU peripherals */

  k1r_clockconfig();

  /* Configure FPU */

  riscv_fpuconfig();

  /* Low level UART setup for debug（必须先于 showprogress，否则
   * riscv_lowputc 读 LSR 等 THRE 时 UART 尚未使能，死循环）
   */

  k1r_lowsetup();

  /* Early serial output (progress) */

  showprogress('A');

#ifdef USE_EARLYSERIALINIT
  riscv_earlyserialinit();
#endif

  showprogress('B');

  /* Board initialization */

  k1r_boardinitialize();

  showprogress('C');

#ifdef CONFIG_BUILD_PROTECTED
  /* 用户态内存初始化：清用户 .bss、拷用户 .data、配置 PMP 内核/用户隔离 */
  k1r_userspace();
  showprogress('D');
#endif

  /* Start NuttX */

  nx_start();

  /* Should never get here */

  for (; ; );
}
