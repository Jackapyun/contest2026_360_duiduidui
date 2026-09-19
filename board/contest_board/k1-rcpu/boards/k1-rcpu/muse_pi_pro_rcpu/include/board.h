/****************************************************************************
 * vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/include/board.h
 ****************************************************************************/

#ifndef __VENDOR_SPACEMIT_BOARDS_K1R_MUSE_PI_PRO_RCPU_INCLUDE_BOARD_H
#define __VENDOR_SPACEMIT_BOARDS_K1R_MUSE_PI_PRO_RCPU_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 时钟：RCPU AON 定时器 131072Hz（esOS SOC_TIMER_FREQ） */
#define BOARD_TIMER_FREQ       (32768 * 4)

/* RCPU UART0（PXA 变体）@0xC0881000，fclk 25.6MHz。
 * 中断：ECLIC 63（Zephyr/esOS 实测一致；手册 rcpu_int 命名相反，待真机核对） */
#define BOARD_UART0_BASE       0xC0881000
#define BOARD_UART0_CLOCK      25600000
#define BOARD_UART0_IRQ        63   /* NuttX IRQ = RISCV_IRQ_ASYNC + 63 = ECLIC ID 63 */

/* NSH 启动参数 */
#define BOARD_LOOPSPERMSEC     (BOARD_TIMER_FREQ / 1000)

#endif /* __VENDOR_SPACEMIT_BOARDS_K1R_MUSE_PI_PRO_RCPU_INCLUDE_BOARD_H */
