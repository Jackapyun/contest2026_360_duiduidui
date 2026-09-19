/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_head.S
 *
 * SpacemiT K1 RCPU (Nuclei N308) 复位启动
 *
 * 参考 esOS startup_gcc.S（Reset_Handler）：
 *   - 关全局中断
 *   - mmisc_ctl：NMI 与 mtvec 共享（NMI_CAUSE_FFF）
 *   - mtvec = NuttX __trap_vec（低 2 位=0b11，ECLIC 模式）
 *   - mtvt2 = __trap_vec（非向量中断公共入口，置 bit0 使能）
 *   - 使能 FPU
 *   - 跳 __k1r_start
 *
 * NuttX 的 __trap_vec（common/riscv_vectors.S）同时处理异常与中断，
 * 64 字节对齐，注释明确支持 CLIC。ECLIC 模式下：
 *   异常 → mtvec；非向量中断（shv=0）→ mtvt2。两者都指向 __trap_vec。
 ****************************************************************************/

#include <nuttx/config.h>

#include <arch/arch.h>
#include <arch/irq.h>

#include "chip.h"
#include "riscv_internal.h"
#include "riscv_macros.S"

/* Nuclei 自定义 CSR 编号（N300/N308 系列，参考 esOS riscv_encoding.h） */
#define K1R_CSR_MTVT        0x307
#define K1R_CSR_MTVT2       0x7EC
#define K1R_CSR_MMISC_CTL   0x7D0
#define K1R_CSR_MCACHE_CTL  0x7CA

#define K1R_MMISC_CTL_NMI_CAUSE_FFF  (1 << 9)
#define K1R_MMISC_CTL_BPU            (1 << 3)

#define K1R_MSTATUS_FS_MASK          0x00006000
#define K1R_MSTATUS_FS_INITIAL       0x00002000

/* ECLIC 模式：mtvec 低 6 位清 0 后置 0x3 */
#define K1R_MTVEC_MODE_MASK           0x3F
#define K1R_MTVEC_MODE_ECLIC          0x3

/* mtvt2 使能位 */
#define K1R_MTVT2_EN                  0x1

  .section .text
  .global __start
  .type   __start, %function

__start:

  /* RCPU 单核：K1 把 RCPU 的 mhartid 硬编码为 1（非 0），原 SMP 多核检查
   * 会把它当从核进入 wfi 死循环。这里强制按 hartid=0 单核启动，使用
   * 第一个 idle 栈。
   */

  li   a0, 0

2:
  /* Set stack pointer to the idle thread stack (from linker layout) */

  riscv_set_inital_sp _ebss, SMP_STACK_SIZE, a0

  /* Disable all interrupts in mie */

  csrw CSR_IE, zero

  /* Disable global interrupt */

  csrci CSR_MSTATUS, MSTATUS_MIE

  /*
   * Set the NMI base mnvec to share with mtvec:
   * mmisc_ctl.NMI_CAUSE_FFF = 1
   */

  li   t0, K1R_MMISC_CTL_NMI_CAUSE_FFF
  csrs K1R_CSR_MMISC_CTL, t0

  /* mtvt = vector table base (share with __trap_vec, not used when shv=0) */

  la   t0, __trap_vec
  csrw K1R_CSR_MTVT, t0

  /* mtvt2 = non-vectored interrupt common entry = __trap_vec, enable it */

  la   t0, __trap_vec
  csrw K1R_CSR_MTVT2, t0
  csrs K1R_CSR_MTVT2, K1R_MTVT2_EN

  /* mtvec = exception entry, ECLIC mode (low 6 bits = 0b11) */

  la   t0, __trap_vec
  csrw CSR_MTVEC, t0
  li   t1, K1R_MTVEC_MODE_MASK
  csrc CSR_MTVEC, t1
  li   t1, K1R_MTVEC_MODE_ECLIC
  csrs CSR_MTVEC, t1

  /* Enable FPU if compiled with float */

#if defined(__riscv_flen) && (__riscv_flen > 0)
  li   t1, K1R_MSTATUS_FS_MASK
  csrc CSR_MSTATUS, t1
  li   t1, K1R_MSTATUS_FS_INITIAL
  csrs CSR_MSTATUS, t1
#endif

  /* Enable I/D-Cache (N308, esOS 同款). 默认 IC_EN/DC_EN=0(缓存关)，此处打开。
   * 注：若需把设备/共享内存标 non-cacheable，可用 MNOCM/MNOCB(0x7F6/0x7F5)；
   * esOS 无 OPENAMP 时仅使能缓存即可。 */

  li   t0, (1 << 0) | (1 << 16)   /* IC_EN(bit0) | DC_EN(bit16) */
  csrs K1R_CSR_MCACHE_CTL, t0
  fence
  fence.i

  /* Jump to C start */

  jal  x1, __k1r_start

  /* Should never return */

  j    __start

  .size __start, . - __start
