/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_irq_dispatch.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) 中断分发
 *
 * NuttX common 的 riscv_exception_common.S 在 mcause 符号位=1（中断）时
 * 调用 riscv_dispatch_irq(vector, regs)。ECLIC 模式下 vector 低 12 位
 * 即 ECLIC 中断 ID，NuttX IRQ = RISCV_IRQ_ASYNC + ID。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>

#include "riscv_internal.h"
#include "chip.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: riscv_dispatch_irq
 ****************************************************************************/

void *riscv_dispatch_irq(uintptr_t vector, uintptr_t *regs)
{
  int irq;

  /* 区分中断（mcause bit31=1）与异常（bit31=0）。
   * CONFIG_LIB_SYSCALL 关闭时 exception_common 不做中断/异常分流，
   * 异常也会进到这里；若不做区分，异常码会被误当 ECLIC 中断 ID，
   * 反复打印 "irq_unexpected_isr"（见 issue-log I-29）。
   */

  if ((vector & RISCV_IRQ_BIT) != 0)
    {
      /* 中断：mcause 低 12 位 = ECLIC 中断 ID */

      irq = RISCV_IRQ_ASYNC + (int)(vector & 0xfff);

      /* Acknowledge the interrupt */

      riscv_ack_irq(irq);

      /* Deliver the IRQ */

      regs = riscv_doirq(irq, regs);
    }
  else
    {
      /* 异常：NuttX IRQ 号 = 异常码（低 12 位，0-15）。
       * 必须走 riscv_doirq 分发到对应 handler（g_irqvector[异常码]）：
       *   - ECALLM(11)/ECALLU(8) → riscv_swint(dispatch_syscall)，
       *     处理 SYS_switch_context/SYS_assert_handler 等（M-mode 无 S 时
       *     riscv_exception_attach 把 ECALLM 注册为 riscv_swint）。
       *   - 其他异常 → riscv_exception 打印 PANIC。
       * 此前（I-30）这里直接调 riscv_exception，绕过了 riscv_doirq 的
       * 分发，导致 context switch/assert 的 ecall 被误判成异常 PANIC。
       */

      irq = (int)(vector & 0xfff);
      regs = riscv_doirq(irq, regs);
    }

  return regs;
}
