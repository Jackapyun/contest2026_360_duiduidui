/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_userspace.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) PROTECTED build 用户态/内存保护初始化
 *
 * 参考 c906_userspace.c（no-MMU + MPU PROTECTED RISC-V）：
 *   - k1r_userspace(): 清用户 .bss、拷贝用户 .data、配置 PMP（8 条 TOR）
 *   - up_allocate_kheap(): 内核堆（M-only）
 * 注意：N308 PMP 仅支持 TOR（无 NAPOT），且 pmpaddr 为 4 字节单位（boundary>>2，
 *      真机 Gate0 实测）。因此 configure_mpu 全部用 PMPCFG_A_TOR。
 *      布局：内核 [0x30000000, CONFIG_NUTTX_USERSPACE)，用户 [USERSPACE, USERSPACE_SIZE)。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>

#include <nuttx/userspace.h>
#include <nuttx/arch.h>

#include "riscv_internal.h"
#include "chip.h"

#ifdef CONFIG_BUILD_PROTECTED

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 用户空间尺寸（默认 1MB；含用户 code/data/heap/stack） */
#ifndef CONFIG_NUTTX_USERSPACE_SIZE
#  define CONFIG_NUTTX_USERSPACE_SIZE        (0x00100000)
#endif

/* 内核运行区终点 = 用户空间基址（内核堆/内核栈/中断栈都在此之下） */
#define K1R_KERNEL_END        CONFIG_NUTTX_USERSPACE

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mpu_set_region
 *
 * Description:
 *   封装 riscv_config_pmp_region 并检查返回值。PMP 是隔离内核/用户的关键
 *   配置，失败不能静默吞掉，否则会出现"看起来隔离了、实际漏了"的隐患。
 *   （第一版只用 riscv_config_pmp_region 却未检查返回值，导致 entry2 静默失败。）
 ****************************************************************************/

static void mpu_set_region(uintptr_t region, uintptr_t attr,
                           uintptr_t base, uintptr_t size)
{
  int ret = riscv_config_pmp_region(region, attr, base, size);
  if (ret != OK)
    {
      PANIC();
    }
}

/****************************************************************************
 * Name: configure_mpu
 *
 * Description:
 *   配置 PMP 把内核与用户态隔离。N308 只支持 TOR（4 字节单位），
 *   按 "底→顶" 顺序排 TOR 边界，只把用户区设为 U 可 RWX。
 *   TOR 语义：riscv_config_pmp_region() 的第 3 参是"边界"（写 pmpaddr=base>>2），
 *   第 4 参 size 对 TOR 忽略；覆盖范围 = [前一条 pmpaddr, 本条 pmpaddr)。
 ****************************************************************************/

static void configure_mpu(void)
{
  /* 1. [0, CONFIG_NUTTX_USERSPACE) : 内核+全集, U 不可访问（仅 M）。
   *    属性为纯 TOR（无 R/W/X) => U/S 访问此区间被拒；不带 L，M 态不受限。 */
  mpu_set_region(0, PMPCFG_A_TOR, CONFIG_NUTTX_USERSPACE, 0);

  /* 2. [USERSPACE, USERSPACE+SIZE) : 用户区, U 可 RWX */
  mpu_set_region(1, PMPCFG_A_TOR | PMPCFG_R | PMPCFG_W | PMPCFG_X,
                 CONFIG_NUTTX_USERSPACE + CONFIG_NUTTX_USERSPACE_SIZE, 0);

  /* 3. [USERSPACE+SIZE, 4GB) : U 不可访问兜底。
   *    注意：不能加 L（PMPCFG_L）！L=1 会令本条 PMP 对 M 态也生效，
   *    而内核跑在 M 态、需要访问高位外设（UART/时钟/存储等），加 L 会把
   *    它们全部锁死。故用纯 TOR（无 L、无 RWX）：只对 U/S 态 deny，M 态不受限。
   *    另外边界必须 4 字节对齐：RV32 最大可表示边界为 0xFFFFFFFC，
   *    不能用 UINT32_MAX(0xffffffff)——它 &3 != 0，会被 pmp_check_region_attrs
   *    拒绝（返回 -EINVAL）而静默失败（第一版就是这个坑）。 */
  mpu_set_region(2, PMPCFG_A_TOR,
                 ((uintptr_t)UINT32_MAX & ~(uintptr_t)0x03), 0);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_userspace
 *
 * Description:
 *   清用户 .bss、拷贝用户 .data、配置 PMP。
 *   在内核启动、跳转到用户态（riscv_jump_to_user(nsh_main)）之前由
 *   __k1r_start/板级调用。
 ****************************************************************************/

void k1r_userspace(void)
{
  uint8_t *src;
  uint8_t *dest;
  uint8_t *end;

  /* 清用户 .bss */
  DEBUGASSERT(USERSPACE->us_bssstart != 0 && USERSPACE->us_bssend != 0 &&
              USERSPACE->us_bssstart <= USERSPACE->us_bssend);
  dest = (uint8_t *)USERSPACE->us_bssstart;
  end  = (uint8_t *)USERSPACE->us_bssend;
  while (dest != end)
    {
      *dest++ = 0;
    }

  /* 拷贝用户 .data（源 = 固件内 us_datasource 段） */
  DEBUGASSERT(USERSPACE->us_datasource != 0 &&
              USERSPACE->us_datastart != 0 && USERSPACE->us_dataend != 0 &&
              USERSPACE->us_datastart <= USERSPACE->us_dataend);
  src  = (uint8_t *)USERSPACE->us_datasource;
  dest = (uint8_t *)USERSPACE->us_datastart;
  end  = (uint8_t *)USERSPACE->us_dataend;
  while (dest != end)
    {
      *dest++ = *src++;
    }

  /* 配置 PMP 隔离内核/用户 */
  configure_mpu();
}

#endif /* CONFIG_BUILD_PROTECTED */
