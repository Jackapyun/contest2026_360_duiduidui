/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_timerisr.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) 系统节拍
 *
 * 复用 NuttX common 的 riscv_mtimer（M-mode 直访 mtime/mtimecmp）。
 * RCPU 私有 TIMER @0xE0030000：mtime@+0x0、mtimecmp@+0x8，
 * 时钟 = rcpu_aon_clk = 32768 * 4 = 131072 Hz（esOS SOC_TIMER_FREQ）。
 * 定时器中断 = ECLIC ID 7 = RISCV_IRQ_MTIMER。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>

#include <nuttx/arch.h>
#include <nuttx/timers/arch_alarm.h>

#include "riscv_internal.h"
#include "riscv_mtimer.h"
#include "hardware/k1r_memorymap.h"
#include "hardware/k1r_timer.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 ****************************************************************************/

void up_timer_initialize(void)
{
  struct oneshot_lowerhalf_s *lower;

  lower = riscv_mtimer_initialize(K1R_TIMER_BASE + 0x000,
                                  K1R_TIMER_BASE + 0x008,
                                  RISCV_IRQ_MTIMER,
                                  K1R_TIMER_FREQ);

  DEBUGASSERT(lower);

  up_alarm_set_lowerhalf(lower);
}
