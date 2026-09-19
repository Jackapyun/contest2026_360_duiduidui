/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/hardware/k1r_eclic.h
 *
 * Nuclei N308 ECLIC（增强 CLIC）寄存器定义
 *
 * 依据 Nuclei NMSIS / Zephyr intc_nuclei_eclic 标准布局：
 *   base+0x000  CLICCFG  （nlbits）
 *   base+0x004  CLICINFO （numint / intctlbits）
 *   base+0x00C  CLICMTH
 *   base+0x1000+4*n  CLICCTRL[n] = { INTIP, INTIE, INTATTR, INTCTRL }
 *
 * K1 上 ECLIC 基址 = 0xE0020000（Zephyr dts / IRegion 探测）
 ****************************************************************************/

#ifndef __K1R_ECLIC_H
#define __K1R_ECLIC_H

#include <stdint.h>
#include "k1r_memorymap.h"

#define K1R_ECLIC_CFG          (*(volatile uint8_t *)(K1R_ECLIC_BASE + 0x000))
#define K1R_ECLIC_INFO         (*(volatile uint32_t *)(K1R_ECLIC_BASE + 0x004))
#define K1R_ECLIC_MTH          (*(volatile uint8_t *)(K1R_ECLIC_BASE + 0x00C))

/* CLICINFO 位域 */
#define K1R_ECLIC_INFO_NUMINT_MASK     0x00001FFFU
#define K1R_ECLIC_INFO_INTCTLBITS_SHIFT 21
#define K1R_ECLIC_INFO_INTCTLBITS_MASK 0x01E00000U

/* CLICCFG 位域：bits[6:1] = nlbits */
#define K1R_ECLIC_CFG_NLBITS_SHIFT     1
#define K1R_ECLIC_CFG_NLBITS_MASK      0x7EU

/* 每中断源控制块（4 字节） */
struct k1r_eclic_ctrl_s
{
  volatile uint8_t intip;      /* +0：pending（bit0 IP） */
  volatile uint8_t intie;      /* +1：enable（bit0 IE） */
  volatile uint8_t intattr;    /* +2：bit0 shv / bits[2:1] trg */
  volatile uint8_t intctl;     /* +3：level/priority（有效位见 intctlbits） */
};

#define K1R_ECLIC_CTRL(n) \
  ((volatile struct k1r_eclic_ctrl_s *)(K1R_ECLIC_BASE + 0x1000 + 4 * (n)))

/* INTATTR 位 */
#define K1R_ECLIC_ATTR_SHV             0x01
#define K1R_ECLIC_ATTR_TRIG_MASK       0x06
#define K1R_ECLIC_ATTR_TRIG_LEVEL      0x00
#define K1R_ECLIC_ATTR_TRIG_POSEDGE    0x02

/* 固定中断源（用户手册 §8.3.3） */
#define K1R_ECLIC_IRQ_MSIP             3
#define K1R_ECLIC_IRQ_MTIP             7

/* 外部中断偏移：rcpu_int[n] → ECLIC ID = 19 + n（esOS SOC_EXTERNAL_MAP_TO_ECLIC_IRQn_OFFSET） */
#define K1R_ECLIC_EXT_IRQ_BASE         19

#endif /* __K1R_ECLIC_H */
