/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_irq.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) ECLIC 中断控制
 *
 * 参考：
 *   - Zephyr intc_nuclei_eclic.c（寄存器布局与使能/优先级逻辑）
 *   - esOS riscv-system-init.c / riscv-interrupt.c（初始化流程）
 *   - NuttX rv32m1_irq.c（芯片层 IRQ 接口模板）
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <arch/board/board.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/k1r_eclic.h"
#include "hardware/k1r_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 中断总数 = ECLIC 源数（含内部） */
#define K1R_NUM_ECLIC_SOURCES  K1R_ECLIC_IRQ_COUNT

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t g_nlbits;
static uint8_t g_intctlbits;
static uint8_t g_max_level;
static uint8_t g_max_prio;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_eclic_leftalign8
 ****************************************************************************/

static inline uint8_t k1r_eclic_leftalign8(uint8_t val, uint8_t bits)
{
  return (uint8_t)(val << (8 - bits));
}

/****************************************************************************
 * Name: k1r_eclic_get_info
 ****************************************************************************/

static void k1r_eclic_get_info(void)
{
  uint32_t info = K1R_ECLIC_INFO;

  g_intctlbits = (uint8_t)((info & K1R_ECLIC_INFO_INTCTLBITS_MASK) >>
                           K1R_ECLIC_INFO_INTCTLBITS_SHIFT);

  if (g_intctlbits < 2)
    {
      g_intctlbits = 8;   /* 保守默认 */
    }

  g_nlbits = (uint8_t)((K1R_ECLIC_CFG & K1R_ECLIC_CFG_NLBITS_MASK) >>
                       K1R_ECLIC_CFG_NLBITS_SHIFT);

  if (g_nlbits > g_intctlbits)
    {
      g_nlbits = g_intctlbits;
    }

  g_max_level = (uint8_t)((1U << g_nlbits) - 1);
  g_max_prio  = (uint8_t)((1U << (g_intctlbits - g_nlbits)) - 1);
}

/****************************************************************************
 * Name: k1r_eclic_set_intctl
 *
 * Description:
 *   Set level/priority of one ECLIC source.
 *
 *   Nuclei ECLIC 的 clicintctl 位序（见 eclic.rst「有效位」）：
 *     - level 位于高位（upper nlbits-bit），左对齐；
 *     - priority 位于剩余有效位的低位。
 *   仲裁顺序：level 高者先；同 level 比 priority；同 priority 比源号（小者先）。
 ****************************************************************************/

static void k1r_eclic_set_intctl(int eclic, uint8_t level, uint8_t prio)
{
  uint8_t lvl  = k1r_eclic_leftalign8(level, g_nlbits);
  uint8_t pr   = k1r_eclic_leftalign8(prio > g_max_prio ? g_max_prio : prio,
                     g_intctlbits);
  uint8_t mask = k1r_eclic_leftalign8(
                     (uint8_t)((1U << g_intctlbits) - 1), g_intctlbits);

  K1R_ECLIC_CTRL(eclic)->intctl = (uint8_t)((pr | lvl) | ~mask);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 ****************************************************************************/

void up_irqinitialize(void)
{
  int i;

  /* Disable machine interrupts */

  up_irq_save();

  /* Reset ECLIC global configuration */

  K1R_ECLIC_MTH = 0;

  /* 配置 cliccfg.nlbits = intctlbits（K1 上 =3，见 CLICINFO.INTCTLBITS）。
   * nlbits 决定 clicintctl 里 level 的位宽；默认 0 会导致 level 恒 0，
   * 而 ECLIC 仲裁要求中断 level > mth(=0)，于是所有中断被屏蔽、永不触发。
   * esOS 等价做法：ECLIC_SetCfgNlbits(__ECLIC_INTCTLBITS=3)。 */

  K1R_ECLIC_CFG = (uint8_t)((3 << K1R_ECLIC_CFG_NLBITS_SHIFT) &
                            K1R_ECLIC_CFG_NLBITS_MASK);

  /* 读取 CLICINFO / CLICCFG */

  k1r_eclic_get_info();

  /* Disable & clear all ECLIC interrupt sources */

  for (i = 0; i < K1R_NUM_ECLIC_SOURCES; i++)
    {
      K1R_ECLIC_CTRL(i)->intie   = 0;
      K1R_ECLIC_CTRL(i)->intip   = 0;
      K1R_ECLIC_CTRL(i)->intattr = 0;
      K1R_ECLIC_CTRL(i)->intctl  = 0;
    }

  /* Colorize the interrupt stack for debug purposes */

#if defined(CONFIG_STACK_COLORATION) && CONFIG_ARCH_INTERRUPTSTACK > 15
  {
    size_t intstack_size = (CONFIG_ARCH_INTERRUPTSTACK & ~15);
    riscv_stack_color(g_intstackalloc, intstack_size);
  }
#endif

  /* Attach the common interrupt handler */

  riscv_exception_attach();

#ifndef CONFIG_SUPPRESS_INTERRUPTS

  /* And finally, enable interrupts */

  riscv_color_intstack();
  up_irq_enable();
#endif
}

/****************************************************************************
 * Name: up_disable_irq
 ****************************************************************************/

void up_disable_irq(int irq)
{
  int eclic = irq - RISCV_IRQ_ASYNC;

  if (eclic >= 0 && eclic < K1R_NUM_ECLIC_SOURCES)
    {
      K1R_ECLIC_CTRL(eclic)->intie = 0;
    }
}

/****************************************************************************
 * Name: up_enable_irq
 ****************************************************************************/

void up_enable_irq(int irq)
{
  int eclic = irq - RISCV_IRQ_ASYNC;

  if (eclic >= 0 && eclic < K1R_NUM_ECLIC_SOURCES)
    {
      /* ECLIC 仲裁要求中断 level > mth(=0)，否则 intip 不置位、中断永不
       * 触发（实测：level=0 时 UART 收满数据 RBR/DR=1 但 intip=0）。
       * 统一给最高 level=7；不区分源（曾试过 UART 高/timer 低分级，
       * 对丢字符无效且已撤销）。 */

      k1r_eclic_set_intctl(eclic, g_max_level, 0);
      K1R_ECLIC_CTRL(eclic)->intie = 1;
    }
}

/****************************************************************************
 * Name: up_irq_enable
 *
 * Description:
 *   Return the current interrupt state and enable interrupts
 ****************************************************************************/

irqstate_t up_irq_enable(void)
{
  irqstate_t oldstat;

  /* Read mstatus & set machine interrupt enable (MIE) in mstatus */

  oldstat = READ_AND_SET_CSR(CSR_MSTATUS, MSTATUS_MIE);
  return oldstat;
}

/****************************************************************************
 * Name: up_prioritize_irq
 *
 * Description:
 *   Set the interrupt level and priority of the ECLIC source.
 *   最高 level/priority，保证可抢占/仲裁。
 ****************************************************************************/

int up_prioritize_irq(int irq, int priority)
{
  int eclic = irq - RISCV_IRQ_ASYNC;

  if (eclic < 0 || eclic >= K1R_NUM_ECLIC_SOURCES)
    {
      return -EINVAL;
    }

  k1r_eclic_set_intctl(eclic, g_max_level,
                       (uint8_t)(priority > 0 ? priority - 1 : 0));

  /* 非向量模式（shv=0），电平触发 */

  K1R_ECLIC_CTRL(eclic)->intattr = K1R_ECLIC_ATTR_TRIG_LEVEL;

  return OK;
}

/****************************************************************************
 * Name: riscv_ack_irq
 ****************************************************************************/

void riscv_ack_irq(int irq)
{
  int eclic = irq - RISCV_IRQ_ASYNC;

  if (eclic >= 0 && eclic < K1R_NUM_ECLIC_SOURCES)
    {
      /* 清 ECLIC pending（Nuclei：INTIP 写 0 清除） */

      K1R_ECLIC_CTRL(eclic)->intip = 0;
    }
}
