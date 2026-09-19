/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/include/irq.h
 *
 * SpacemiT K1 RCPU (Nuclei N308) 中断编号
 *
 * RCPU 使用 ECLIC：中断进入时 mcause 低 12 位即 ECLIC 中断 ID，
 * NuttX IRQ = RISCV_IRQ_ASYNC + ECLIC ID。
 * 固定 ID：3=软中断（=RISCV_IRQ_MSOFT=19）、7=定时器（=RISCV_IRQ_MTIMER=23），
 * 与 NuttX 的 machine 中断编号天然一致。
 *
 * 外部中断：ECLIC ID = 19 + rcpu_int[n]（用户手册 §7.3）。
 ****************************************************************************/

#ifndef __VENDOR_SPACEMIT_CHIPS_K1R_INCLUDE_IRQ_H
#define __VENDOR_SPACEMIT_CHIPS_K1R_INCLUDE_IRQ_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ECLIC 中断源 → NuttX IRQ */
#define K1R_IRQ_ECLIC(n)      (RISCV_IRQ_ASYNC + (n))

/* 固定内部中断 */
#define K1R_IRQ_SOFT          K1R_IRQ_ECLIC(3)
#define K1R_IRQ_TIMER         K1R_IRQ_ECLIC(7)

/* 外部中断（rcpu_int[n] → ECLIC 19+n，见用户手册 §7.3） */
#define K1R_IRQ_DMAC          K1R_IRQ_ECLIC(19)
#define K1R_IRQ_AP_WAKEUP     K1R_IRQ_ECLIC(21)
#define K1R_IRQ_VOICE_DET     K1R_IRQ_ECLIC(22)
#define K1R_IRQ_RSM           K1R_IRQ_ECLIC(23)
#define K1R_IRQ_ADMA_CH0      K1R_IRQ_ECLIC(24)
#define K1R_IRQ_ADMA_CH1      K1R_IRQ_ECLIC(25)
#define K1R_IRQ_DFE_SSPA      K1R_IRQ_ECLIC(26)
#define K1R_IRQ_TIMER1        K1R_IRQ_ECLIC(27)
#define K1R_IRQ_TIMER2        K1R_IRQ_ECLIC(28)
#define K1R_IRQ_TIMER3        K1R_IRQ_ECLIC(29)
#define K1R_IRQ_IPC_AP2AUD    K1R_IRQ_ECLIC(30)
#define K1R_IRQ_I2C0          K1R_IRQ_ECLIC(32)
#define K1R_IRQ_I2C1          K1R_IRQ_ECLIC(33)
#define K1R_IRQ_SSP0          K1R_IRQ_ECLIC(34)
#define K1R_IRQ_SSP1          K1R_IRQ_ECLIC(35)
#define K1R_IRQ_UART0         K1R_IRQ_ECLIC(63)
#define K1R_IRQ_AUD_OCP       K1R_IRQ_ECLIC(37)
#define K1R_IRQ_HOOK_KEY      K1R_IRQ_ECLIC(38)
#define K1R_IRQ_AUD_PLUG      K1R_IRQ_ECLIC(39)
#define K1R_IRQ_IPC_MSA2AUD   K1R_IRQ_ECLIC(40)
#define K1R_IRQ_AUD_AP_WKUP   K1R_IRQ_ECLIC(41)
#define K1R_IRQ_SENSOR_IRPC   K1R_IRQ_ECLIC(42)
#define K1R_IRQ_GPIO0_PMIC    K1R_IRQ_ECLIC(43)
#define K1R_IRQ_GPIO1         K1R_IRQ_ECLIC(44)
#define K1R_IRQ_GPIO2         K1R_IRQ_ECLIC(45)
#define K1R_IRQ_GPIO3         K1R_IRQ_ECLIC(46)
#define K1R_IRQ_GPIO4         K1R_IRQ_ECLIC(47)
#define K1R_IRQ_GPIO5         K1R_IRQ_ECLIC(48)
#define K1R_IRQ_GPIO6         K1R_IRQ_ECLIC(49)
#define K1R_IRQ_GPIO7         K1R_IRQ_ECLIC(50)
#define K1R_IRQ_ADMA1_CH1     K1R_IRQ_ECLIC(52)
#define K1R_IRQ_ADMA1_CH0     K1R_IRQ_ECLIC(53)
#define K1R_IRQ_SSPA1         K1R_IRQ_ECLIC(54)
#define K1R_IRQ_HDMI_ADMA     K1R_IRQ_ECLIC(55)
#define K1R_IRQ_ADMA0_CH1     K1R_IRQ_ECLIC(57)
#define K1R_IRQ_ADMA0_CH0     K1R_IRQ_ECLIC(58)
#define K1R_IRQ_SSPA0         K1R_IRQ_ECLIC(59)
#define K1R_IRQ_CAN0_INT0     K1R_IRQ_ECLIC(60)
#define K1R_IRQ_CAN0_INT1     K1R_IRQ_ECLIC(61)
#define K1R_IRQ_IR            K1R_IRQ_ECLIC(62)
#define K1R_IRQ_UART1         K1R_IRQ_ECLIC(36)
#define K1R_IRQ_SHUB_EDGE     K1R_IRQ_ECLIC(64)

/* 总中断数（ECLIC 0..64 + 余量） */
#define K1R_ECLIC_IRQ_COUNT   65
#define NR_IRQS               (RISCV_IRQ_ASYNC + K1R_ECLIC_IRQ_COUNT)

#endif /* __VENDOR_SPACEMIT_CHIPS_K1R_INCLUDE_IRQ_H */
