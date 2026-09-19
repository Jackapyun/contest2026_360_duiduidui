# JTAG 调试 RCPU 并验证串口 NSH

## 目录

- 硬件接线（JTAG + 串口）
- 前置条件：官方系统使能 RCPU
- 启动 OpenOCD
- GDB 加载固件
- 串口验收（A→B→C→nsh>）
- 镜像 >256KB：DDR remap
- 常见问题排障
- 兜底：devmem 手动使能寄存器表

## 硬件接线

### RCPU 调试串口（r_uart0，不在板载串口座上）

- 寄存器 SHUB_UART0 @ `0xC0881000`（PXA/ns16550 变体，需 UUE 位）
- 引脚：**GPIO47 = r_uart0_tx，GPIO48 = r_uart0_rx**，MUX_MODE1、3.3V（PAD_3V_DS4）、PULL_UP
- 波特率 **115200 8N1**
- 接线：USB-TTL（3.3V）TX→GPIO48、RX→GPIO47、GND 共地
- 板载串口座默认接**大核 uart0（0xD4017000）**，查 RCPU 日志需从 GPIO47/48 飞线

### JTAG（Primary JTAG @ 40Pin GPIO 排针）

| 40Pin Pin | 网络 | 信号 |
|---|---|---|
| 7 | GPIO70_3V3 | PRI_TDI |
| 11 | GPIO71_3V3 | PRI_TMS |
| 13 | GPIO72_3V3 | PRI_TCK |
| 15 | GPIO73_3V3 | PRI_TDO |
| 6/9/14/25/30/34/39（任一） | GND | GND |
| 1 或 17 | 3V3 | VTref（J-Link 参考电压） |

J-Link 接线步骤：

1. 拔掉 J-Link 外壳内跳线帽（不对板供电，避免电平冲突）
2. TDI→Pin7、TMS→Pin11、TCK→Pin13、TDO→Pin15、GND→GND、VTref→3V3
3. TRSTn 未引出，用 `monitor reset halt` 复位，无需 TRST

> ⚠️ 线序为 K1 系列通用，MUSE Pi Pro 到手先核对丝印/Pin 号；GND/3V3 以万用表实测为准（Bit-Brick 40Pin 表 Pin20=GPIO49，与树莓派标准不同）。

## 前提：主系统已运行并使能 RCPU

**前提假设：主系统（Bianbu/OH）镜像已烧录并正常运行**（本 skill 不负责烧录主系统）。

为什么需要主系统：RCPU（N308）**不自举**，冷启动由 X60 大核主导（ROM→FSBL→OpenSBI→U-Boot→OS）。只有主系统起来后，AP 才会完成：

1. 给 AUD 电源域上电（RCPU 是独立电源岛）
2. 释放 RCPU 时钟/复位（AUD_MCUSYSCTRL / RESET_AUDIO_SYS）
3. 配好 r_uart0 的 pinctrl（GPIO47/48 → MUX_MODE1）

若 RCPU 电源域没上电，JTAG 扫不到 TAP（CPUTAPID 扫描失败）。

官方 MUSE Pi Pro dts 已默认使能 RCPU 节点（`&rcpu { status="okay" }`、`&r_uart0 { status="okay" }`），rproc 驱动会自动加载官方 `esos.elf` 并释放 RCPU 运行。

验证命令（板端 root shell）：

```bash
# AUD 电源域已上电
cat /sys/kernel/debug/pm_genpd/pm_genpd_summary | grep -A1 SPT_PD_AUDIO
# rproc 设备存在且 running
ls /sys/class/remoteproc/
cat /sys/class/remoteproc/remoteproc*/state
# pinctrl 生效（GPIO47/48 = MUX_MODE1）
cat /sys/kernel/debug/pinctrl/*/pinmux-pins | grep -E "47|48"
```

> 若官方 esos.elf 已加载，建议先停掉避免与 JTAG 冲突：
> `echo stopped > /sys/class/remoteproc/remoteproc0/state`（不停也可，JTAG `reset halt` 后 load 会覆盖内存）。

## 启动 OpenOCD

```bash
cd $OCD_DIR            # ⚠️ 必须在 bin 目录下运行（脚本用相对路径）
./k1-rcpu.sh
```

脚本实际命令（`ADAPTER_DRIVER=jlink`，CMSIS-DAP 时改该变量）：

```bash
./openocd -c "bindto 0.0.0.0" -c "gdb port 1024" -c "telnet port 4444" \
          -c "set SPEED 8000" -c "set SECJTAG 0" -c "set TARGET rcpu" \
          -f $SCRIPT_DIR/interface/$ADAPTER_DRIVER.cfg \
          -f $SCRIPT_DIR/spacemit_helper.tcl \
          -f scripts/spacemit-k1.cfg
```

要点：`TARGET=rcpu` → CPUTAPID=`0x10308A6D`、DRVAL=`0xE`（大核 X60 为 `0x10000E21`）；目标名 `k1.cpu_rcpu.0`。

**成功标志**：输出 `target created: k1.cpu_rcpu.0`（或 TAP 扫描 + target examine 通过）。

## GDB 加载固件

```bash
cd $VELA_WORK/cmake_out/muse_pi_pro_rcpu_nsh   # 或 muse_pi_pro_rcpu_nsh-min
$GDB nuttx
```

GDB 内顺序执行：

```
set pagination off
target remote localhost:1024
monitor reset halt          # 复位并停在复位向量
load                        # 按 ELF section 写入 DDR（0x30000000 布局），PC 设到入口
monitor reg pc              # 核对 PC == 0x30000100（DDR remap 布局）
continue
```

备选（telnet 方式，不用 gdb）：

```
telnet localhost 4444
halt
load_image nuttx 0x30000000 0  # ELF 加载（DDR remap；或 load_image nuttx.bin 0x30000000 bin）
resume 0x30000100
```

常用调试命令：

| 操作 | 命令 |
|---|---|
| 看寄存器 | `info reg` |
| 看 N308 自定义 CSR | `p/x $csr1498` 等 |
| 内存查看 | `x/32wx 0x0`、`x/8i $pc` |
| 断点/单步 | `break __k1r_start`、`si` |
| 看 mcause/mepc | `info registers mepc mcause mtval`（⚠️ CSR 需逐个或在 `info registers` 里列名；`p/x $mcause $mepc` 一次性读多寄存器是 GDB 非法语法） |
| 看 mstatus 等 | `p/x $mstatus`（单个寄存器可 `p/x`） |

## 串口验收（A→B→C→nsh>）

固件启动进度字符（k1r_start.c 的 showprogress）：

- `A`：进入 `__k1r_start`（清 BSS/搬 data 完成）
- `B`：`riscv_earlyserialinit()` 完成
- `C`：板级初始化完成，即将 `nx_start()`

r_uart0（115200）依次看到 `A`→`B`→`C`，随后出现 `nsh>` 提示符 = bringup 成功。验证：`help` / `ps` / `uname -a`。

异常 dump 解读：ECLIC 模式下 mcause 低 12 位 = 中断 ID（3=软、7=定时器、19+=外部）；异常时为标准异常码（0-15）。

## 镜像 >256KB：DDR remap

SRAM 仅 256KB。若 `nuttx.bin` 超限：

1. AP 侧先写 `DDR_REMAP_BASE=0x100000`：`devmem 0xC08800C0 32 0x100000`
2. 固件按 RCPU 视角 `0x30300000` 附近链接（官方 esOS entry `0x30300114` 同款）
3. GDB `load` 到 `0x30300000` 段，`continue`

## 常见问题排障

| 现象 | 原因/处理 |
|---|---|
| GDB 连不上 1024 | OpenOCD 没起来或端口被占；`ss -tlnp \| grep 1024` |
| OpenOCD 扫不到 TAP | RCPU 没上电（先跑官方系统）；JTAG 线序/电平/共地 |
| load 后无输出 | pinctrl 未生效（GPIO47/48 非 MUX_MODE1）；TX/RX 接反；波特率非 115200 |
| 串口乱码 | UART 分频/时钟不对：核对 `0xC08800D8` 的 FCLK_SEL 与固件内时钟（25.6M/115200，除数≈14 需按实际分频校正） |
| RCPU 不运行 | `MCU_EXECUTION_CTRL` 未置 1：`devmem 0xC088C030 32 1` |
| 官方 esos 干扰 | `echo stopped > /sys/class/remoteproc/remoteproc0/state` 后再 halt/load |
| 地址越界/load 报错 | 固件链接地址与 load 目标不一致：SRAM=0x0，DDR remap=0x30300000 |
| PXA UART 无收发 | IER 的 UUE 位必须置 1（NuttX 16550 需补） |
| 卡在 A | 串口初始化前异常（查 head.S mtvec/ECLIC 配置） |
| 卡在 B | 16550 驱动注册问题（IRQ/地址/访问宽度） |
| 无 tick | ECLIC ID7 定时器中断未使能/未清 pending |
| 中断风暴 | ECLIC INTIP 未 ack；电平触发源未拉低 |

## 兜底：devmem 手动使能寄存器表

仅当 rproc 驱动缺失或需精确控制时用（板端 root shell，先读后写避免破坏其他位）。

> ⚠️ 这些是**运行时 volatile 寄存器操作**，断电即恢复，**不写任何持久存储、不会破坏主系统镜像**；但会临时改变主系统正在管理的 RCPU 电源/时钟状态，调试结束后复位主系统即可还原。

```bash
rw32() { addr=$1; v=$(devmem $addr 32); devmem $addr 32 $((v | $2)); }
clr32() { addr=$1; v=$(devmem $addr 32); devmem $addr 32 $((v & ~$2)); }
```

| 目的 | 地址 | 位域/操作 |
|---|---|---|
| UART0 时钟+复位 | `0xC08800D8` | [5:4] FCLK_SEL（01=25.6M）；[2] PCLK_EN；[1] FCLK_EN；[0] SW_RSTN → 推荐写 `0x17` |
| audio 源时钟 | `0xD428294C` | [12] gate 使能 → `rw32 ... 0x1000` |
| AUD_PMU 防掉电 | `0xC088C018` | [3:1] PWR_OFF/PLL_OFF/LP → `clr32 ... 0xE` |
| MCU 运行控制 | `0xC088C030` | [0] mcu_execution_ctrl=1 → `devmem ... 1` |
| DDR_REMAP_BASE | `0xC08800C0` | 32 位 remap 基址（官方 0x100000） |
| IPC2AP 邮箱（L0 可跳过） | `0xC088C02C` | [1] ipc2ap clk en；[0] aipc_ap_rstn → `devmem ... 3` |

> ⚠️ 两个待真机核对项：
> 1. AUD_PMU 基址：手册写 `0xC0A10000`，Linux dts/esOS 用 `0xC088C000` → 以 dts/esOS 为准，真机读回核对。
> 2. pinctrl 位域编码：优先用官方系统已生效的值，devmem 只改 MUX 位。
