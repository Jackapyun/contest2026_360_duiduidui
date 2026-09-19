/****************************************************************************
 * vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/src/k1r_boardinit.c
 *
 * SpacemiT MUSE Pi Pro RCPU（N308）板级初始化
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/board.h>
#include <nuttx/clock.h>
#include <arch/board/board.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_boardinitialize
 ****************************************************************************/

void k1r_boardinitialize(void)
{
  /* RCPU 板级硬件初始化（LED/GPIO 等，真机验证阶段补充） */
}

/****************************************************************************
 * Name: board_early_initialize
 ****************************************************************************/

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
  k1r_boardinitialize();
}
#endif

/****************************************************************************
 * Name: board_late_initialize
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  /* 串口等设备由 nx_start 内的 riscv_serialinit() 完成 */
}
#endif
