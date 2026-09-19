/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/hardware/k1r_memorymap.h
 *
 * SpacemiT K1 RCPU (Nuclei N308) 内存与外设地址映射
 *
 * 依据：
 *   - K1 用户手册 §6.3（RCPU 域地址映射）
 *   - Zephyr PR #95248 dts/riscv/spacemit/k1_n308.dtsi
 *     （ECLIC@0xE0020000、SYSTIMER@0xE0030000、SRAM@0x0 256KB）
 *   - esOS bsp/spacemit/platform/n308/k1-x/gcc.ld（DDR remap 布局）
 ****************************************************************************/

#ifndef __K1R_MEMORYMAP_H
#define __K1R_MEMORYMAP_H

/* RCPU 私有 SRAM：256KB（用户手册 §6.3） */
#define K1R_SRAM_BASE          0x00000000
#define K1R_SRAM_SIZE          0x00040000

/* RCPU 访问 DDR 的窗口（经 DDR_REMAP_BASE 重映射到大核 DDR） */
#define K1R_DDR_WINDOW_BASE    0x30000000
#define K1R_DDR_WINDOW_SIZE    0x10000000

/* 官方 esOS 入口（DDR remap 窗口内）与资源表 */
#define K1R_ESOS_ENTRY_POINT   0x30300114
#define K1R_RSC_TABLE_BASE     0x302FC000
#define K1R_RSC_TABLE_SIZE     0x00004000

/* Nuclei 私有外设（IRegion 探测结果，与 Zephyr dts 一致） */
#define K1R_IRegion_BASE       0xE0000000
#define K1R_ECLIC_BASE         0xE0020000
#define K1R_ECLIC_SIZE         0x00002000
#define K1R_TIMER_BASE         0xE0030000
#define K1R_TIMER_SIZE         0x00001000

/* RCPU 域外设（用户手册 §6.3） */
#define K1R_CAN_BASE           0xC0870000
#define K1R_CAN_SIZE           0x00004000
#define K1R_SYSCTRL_BASE       0xC0880000   /* AUD_MCUSYSCTRL：时钟/复位/DDR_REMAP */
#define K1R_UART0_BASE         0xC0881000   /* SHUB_UART0（PXA 变体） */
#define K1R_AUDCLK_BASE        0xC0882000   /* AUD_AUDCLOCK */
#define K1R_ADMA_BASE          0xC0883000   /* CODEC ADMA */
#define K1R_SSPA_BASE          0xC0883100   /* CODEC SSPA */
#define K1R_AHBDMA_BASE        0xC0884000   /* 16 通道 DMA */
#define K1R_SSP0_BASE          0xC0885000   /* SHUB_SSP0（SPI） */
#define K1R_SSP1_BASE          0xC0886000   /* SHUB_SSP1（SPI） */
#define K1R_I2C0_BASE          0xC0887000
#define K1R_PWM_BASE           0xC0888000   /* PWM0-9 */
#define K1R_AON_TIMER_BASE     0xC0889000   /* 3 计数器 × 3 匹配值 */
#define K1R_AON_IPC2AP_BASE    0xC088A000   /* RCPU→AP 邮箱 */
#define K1R_AON_PMU_BASE       0xC088C000
#define K1R_UART1_BASE         0xC088D000   /* SHUB_UART1（带自动流控） */
#define K1R_IR_BASE            0xC088E000   /* R_IR_RX */
#define K1R_AUD_BUFFER_BASE    0xC08D0000   /* Audio Buffer */
#define K1R_AUD_PMU_BASE       0xC0A10000   /* AUD_PMU（电源投票/唤醒/MCU 执行控制） */

#endif /* __K1R_MEMORYMAP_H */
