/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/hardware/k1r_ccu.h
 *
 * RCPU 时钟/复位/电源/执行控制寄存器
 *
 * 依据：
 *   - K1 用户手册 §14.3（AUD_PMU @0xC0A10000、AUD_MCUSYSCTRL @0xC0880000）
 *   - K1 用户手册 §9（AUDIO_CLK_RES_CTRL 等）
 ****************************************************************************/

#ifndef __K1R_CCU_H
#define __K1R_CCU_H

#include <stdint.h>
#include "k1r_memorymap.h"

/* AUD_MCUSYSCTRL（0xC0880000）寄存器偏移 */
#define K1R_SYSCTRL_SSP0_CLK_RST      0x28
#define K1R_SYSCTRL_I2C0_CLK_RST      0x30
#define K1R_SYSCTRL_UART1_CLK_RST     0x3C
#define K1R_SYSCTRL_CAN_CLK_RST       0x48
#define K1R_SYSCTRL_IR_CLK_RST        0x4C
#define K1R_SYSCTRL_DDR_REMAP_BASE    0xC0
#define K1R_SYSCTRL_UART0_CLK_RST     0xD8

/* 通用 CLK/RST 字段 */
#define K1R_CLK_FCLK_SEL_SHIFT        4
#define K1R_CLK_FCLK_SEL_MASK         (0x3 << K1R_CLK_FCLK_SEL_SHIFT)
/* ⚠️ 权威频率（Linux/esOS ccu ruart0_parent_names）：
 *   0=pll1_aud_24p5(24.5M), 1=pll1_aud_245p7(245.7M), 2=vctcxo_24(24M), 3=vctcxo_3(3M)
 * 手册 §14 的 62M/26M/13M/3.25M 命名不符（sel=1 实为 245.7MHz，误用致 UART 发送失效）
 */
#define K1R_CLK_FCLK_SEL_24P5M        0
#define K1R_CLK_FCLK_SEL_245P7M       1
#define K1R_CLK_FCLK_SEL_24M          2
#define K1R_CLK_FCLK_SEL_3M           3
#define K1R_CLK_FCLK_DIV_SHIFT        8
#define K1R_CLK_FCLK_DIV_MASK         (0x7FF << K1R_CLK_FCLK_DIV_SHIFT)
#define K1R_CLK_PCLK_EN               (1 << 2)
#define K1R_CLK_FCLK_EN               (1 << 1)
#define K1R_CLK_SW_RSTN               (1 << 0)

#define K1R_DDR_REMAP_BASE_REG \
  (*(volatile uint32_t *)(K1R_SYSCTRL_BASE + K1R_SYSCTRL_DDR_REMAP_BASE))

/* APBS（0xD4090000）— APB_SPARE2_REG（0x104）：pll1_aud 分频时钟 gate
 *   bit10 = pll1_aud_245p7(245.76MHz)，bit11 = pll1_aud_24p5(24.576MHz)
 *   UART0 用 sel=0 → pll1_aud_24p5，必须使能 bit11，否则 fclk=0 发送卡死。
 */
#define K1R_APBS_BASE               0xD4090000
#define K1R_APBS_SPARE2             (K1R_APBS_BASE + 0x104)
#define K1R_PLL1_AUD_24P5_GATE      (1 << 11)

/* AUD_PMU（0xC0A10000）寄存器偏移 */
#define K1R_AUDPMU_VOTE                0x18
#define K1R_AUDPMU_VOTE_MAIN           0x20
#define K1R_AUDPMU_WAKEUP_EN           0x28
#define K1R_AUDPMU_AON_CLK_RST         0x2C
#define K1R_AUDPMU_MCU_EXEC_CTRL       0x30
#define K1R_AUDPMU_BUS_CLK_DIV         0x38

/* MCU_EXECUTION_CTRL：1=运行，0=暂停（AP 侧控制 RCPU 启停） */
#define K1R_MCU_EXEC_CTRL_REG \
  (*(volatile uint32_t *)(K1R_AUD_PMU_BASE + K1R_AUDPMU_MCU_EXEC_CTRL))

/* AUD_AUDCLOCK（0xC0882000）：SSPA/DFE/Codec 时钟 */
#define K1R_AUDCLK_CODEC_TXRX_CTRL     0x14
#define K1R_AUDCLK_DFE_CTRL            0x1C

/* RCPU 核心时钟（rcpu_clk / CLK_AUDIO）—— APMU 域。
 * 寄存器 = APMU_RCPU_CLK_RES_CTRL = APMU_BASE(0xD4282800) + 0x14C = 0xD428294C。
 * 位域（esOS ccu_mix DIV_FC_MUX_GATE，与手册 §9 AUDIO_CLK_RES_CTRL 一致）：
 *   MUX(选源) bit[9:7]、DIV bit[6:4]、GATE(音频EN) bit12、FC bit15。
 * 时钟源：0=pll1_aud_245p7(245.7M, N308 标称)、1=pll1_d8_307p2、
 *         2=pll1_d5_491p52(最大)、3=pll1_d6_409p6。
 */
#define K1R_APMU_BASE                 0xD4282800
#define K1R_RCPU_CLK_RES_CTRL         (K1R_APMU_BASE + 0x14C)
#define K1R_RCPU_CLK_MUX_SHIFT        7
#define K1R_RCPU_CLK_MUX_MASK         (0x7 << K1R_RCPU_CLK_MUX_SHIFT)
#define K1R_RCPU_CLK_DIV_SHIFT        4
#define K1R_RCPU_CLK_DIV_MASK         (0x7 << K1R_RCPU_CLK_DIV_SHIFT)
#define K1R_RCPU_CLK_SRC_245P7        0   /* pll1_aud_245p7: N308 标称 245.7 MHz */
#define K1R_RCPU_CLK_SRC_307P2        1
#define K1R_RCPU_CLK_SRC_491P52       2   /* pll1_d5_491p52: 最高, ~2x */
#define K1R_RCPU_CLK_SRC_409P6        3
#define K1R_RCPU_CLK_SRC              K1R_RCPU_CLK_SRC_245P7  /* 默认 N308 标称 */
#define K1R_RCPU_CLK_DIV              0   /* div by 1 */

#endif /* __K1R_CCU_H */
