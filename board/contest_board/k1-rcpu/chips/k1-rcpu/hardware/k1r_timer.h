/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/hardware/k1r_timer.h
 *
 * Nuclei N308 私有 64 位实时定时器（TIMER）
 *
 * 基址 0xE0030000（Zephyr dts：mtime@+0x0、mtimecmp@+0x8）
 * 时钟频率：rcpu_aon_clk = 32768 * 4 = 131072 Hz（esOS SOC_TIMER_FREQ）
 ****************************************************************************/

#ifndef __K1R_TIMER_H
#define __K1R_TIMER_H

#include <stdint.h>
#include "k1r_memorymap.h"

#define K1R_TIMER_MTIME       (*(volatile uint64_t *)(K1R_TIMER_BASE + 0x000))
#define K1R_TIMER_MTIMECMP    (*(volatile uint64_t *)(K1R_TIMER_BASE + 0x008))
#define K1R_TIMER_MSIP        (*(volatile uint32_t *)(K1R_TIMER_BASE + 0x010))
#define K1R_TIMER_MSFTRST     (*(volatile uint32_t *)(K1R_TIMER_BASE + 0x018))

/* 定时器时钟频率（AON 域） */
#define K1R_TIMER_FREQ        (32768 * 4)

/* 软复位魔数（用户手册 §8.3.3） */
#define K1R_TIMER_MSFTRST_MAGIC 0x80000A5FU

#endif /* __K1R_TIMER_H */
