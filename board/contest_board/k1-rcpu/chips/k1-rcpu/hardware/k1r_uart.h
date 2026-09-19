/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/hardware/k1r_uart.h
 *
 * SpacemiT K1 RCPU (Nuclei N308) SHUB_UART0 寄存器定义
 *
 * SHUB_UART0 @0xC0881000（PXA UART 变体，ns16550 兼容 + PXA 扩展位）
 * 依据：K1 用户手册 16.3 UART 章节（权威）+ esOS pxa_uart.h + Linux pxa_k1x.c
 *
 * 寄存器偏移为 32 位总线地址（手册 offset: IER=0x4, IIR/FCR=0x8, LCR=0xC,
 * MCR=0x10 ...，与 ns16550 兼容但按 32 位对齐）。
 ****************************************************************************/

#ifndef __K1R_UART_H
#define __K1R_UART_H

#include <stdint.h>
#include "k1r_memorymap.h"

/* ns16550 寄存器偏移（32 位访问） */
#define K1R_UART_RBR_THR_DLL   0x00
#define K1R_UART_IER_DLM       0x04
#define K1R_UART_FCR_IIR       0x08
#define K1R_UART_LCR           0x0C
#define K1R_UART_MCR           0x10
#define K1R_UART_LSR           0x14
#define K1R_UART_MSR           0x18
#define K1R_UART_SCR           0x1C

/* ---- IER（手册 16.3 IER，offset 0x4）---- */
#define K1R_UART_IER_RAVIE     (1 << 0)   /* Receiver Data Available Int Enable */
#define K1R_UART_IER_TIE       (1 << 1)   /* Transmit Data Request Int Enable */
#define K1R_UART_IER_RLSE      (1 << 2)   /* Receiver Line Status Int Enable */
#define K1R_UART_IER_MIE       (1 << 3)   /* Modem Interrupt Enable */
#define K1R_UART_IER_RTOIE     (1 << 4)   /* Receiver Time-out Int Enable (PXA) */
#define K1R_UART_IER_NRZE      (1 << 5)   /* NRZ Coding Enable */
#define K1R_UART_IER_UUE       (1 << 6)   /* UART Unit Enable */
#define K1R_UART_IER_DMAE      (1 << 7)   /* DMA Requests Enable */

/* 中断模式完整使能（RDA+RLS+RTO+UUE）。RTOIE 是 PXA RDA 硬件触发前提，
 * 仅使能 RAVIE 时 RX 中断不上报（GDB 实测 IIR 恒 NO_INT）。 */
#define K1R_UART_IER_INTMODE   (K1R_UART_IER_RAVIE | K1R_UART_IER_RLSE | \
                                K1R_UART_IER_RTOIE | K1R_UART_IER_UUE)

/* ---- FCR（手册 16.3 FCR，offset 0x8）---- */
#define K1R_UART_FCR_TRFIFOE   (1 << 0)   /* Transmit & Receive FIFO Enable */
#define K1R_UART_FCR_RESETRF   (1 << 1)   /* Reset Receive FIFO */
#define K1R_UART_FCR_RESETTF   (1 << 2)   /* Reset Transmit FIFO */
#define K1R_UART_FCR_TIL       (1 << 3)   /* Transmitter Int Level (0=FIFO half empty) */
#define K1R_UART_FCR_TRAIL     (1 << 4)   /* Trailing Bytes (0=removed by K1) */
#define K1R_UART_FCR_BUS       (1 << 5)   /* 32-Bit Peripheral Bus */
#define K1R_UART_FCR_ITL_SHIFT 6
#define K1R_UART_FCR_ITL_1B    (0 << 6)   /* interrupt when >=1 byte */
#define K1R_UART_FCR_ITL_8B    (1 << 6)
#define K1R_UART_FCR_ITL_16B   (2 << 6)
#define K1R_UART_FCR_ITL_32B   (3 << 6)

/* 双 FIFO + 1 字节 RX 触发阈值 + TX 半空中断。
 * 注意：BUS 位保持 0（8 位外设总线）——手册 16.3 "32-Bit Peripheral Bus"，
 * BUS=0 时只有低字节有效，写 THR 只发 1 字节；BUS=1（32 位外设总线）时写
 * THR 会把整个 32 位字的 4 字节都发出（实测每个字符后跟 3 个 0x00）。
 * 寄存器偏移仍是 32 位对齐（0x4/0x8/...），由 reg-shift=2 处理。 */
#define K1R_UART_FCR_VAL       (K1R_UART_FCR_TRFIFOE | K1R_UART_FCR_ITL_1B)

/* ---- IIR（手册 16.3 IIR，offset 0x8，只读）---- */
#define K1R_UART_IIR_NIP       (1 << 0)   /* 0 = interrupt pending (active low) */
#define K1R_UART_IIR_IID_SHIFT 1
#define K1R_UART_IIR_IID_MASK  (0x3 << 1) /* IID10 bits[2:1] */
#define K1R_UART_IIR_IID_MS    (0 << 1)   /* Modem status */
#define K1R_UART_IIR_IID_TX    (1 << 1)   /* Transmit FIFO requests data */
#define K1R_UART_IIR_IID_RDA   (2 << 1)   /* Received data available */
#define K1R_UART_IIR_IID_RLS   (3 << 1)   /* Receive error (overrun/parity/framing/break/FIFO err) */
#define K1R_UART_IIR_TOD       (1 << 3)   /* Time Out Detected (FIFO mode) */
#define K1R_UART_IIR_FIFO      (0xC0)     /* FIFO mode enable status (bit7:6=11) */

/* ---- LCR（手册 16.3 LCR，offset 0xC）---- */
#define K1R_UART_LCR_WLS       (0x3 << 0) /* Word length */
#define K1R_UART_LCR_WLS_8     (0x3 << 0) /* 8-bit */
#define K1R_UART_LCR_STB       (1 << 2)   /* 1 stop bit */
#define K1R_UART_LCR_PEN       (1 << 3)   /* Parity enable */
#define K1R_UART_LCR_EPS       (1 << 4)   /* Even parity select */
#define K1R_UART_LCR_STKYP     (1 << 5)   /* Sticky parity */
#define K1R_UART_LCR_SB        (1 << 6)   /* Set break */
#define K1R_UART_LCR_DLAB      (1 << 7)   /* Divisor Latch Access Bit */

/* ---- MCR ---- */
#define K1R_UART_MCR_OUT2      (1 << 3)   /* 中断输出/发送使能关键位 */

/* ---- LSR（手册 16.3 LSR）---- */
#define K1R_UART_LSR_DR        (1 << 0)   /* Data Ready (>=1 byte in RX FIFO) */
#define K1R_UART_LSR_OE        (1 << 1)   /* Overrun Error */
#define K1R_UART_LSR_PE        (1 << 2)   /* Parity Error */
#define K1R_UART_LSR_FE        (1 << 3)   /* Framing Error */
#define K1R_UART_LSR_BI        (1 << 4)   /* Break Interrupt */
#define K1R_UART_LSR_THRE      (1 << 5)   /* Transmit Data Request (FIFO half-empty) */
#define K1R_UART_LSR_TEMT      (1 << 6)   /* Transmitter Empty (FIFO empty) */
#define K1R_UART_LSR_FIFOE     (1 << 7)   /* FIFO Error Status */

#endif /* __K1R_UART_H */
