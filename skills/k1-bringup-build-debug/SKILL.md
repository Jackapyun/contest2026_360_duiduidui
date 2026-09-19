---
name: k1-bringup-build-debug
description: 编译、启动(JTAG 烧录)、串口、调试全流程 for openvela/NuttX on SpacemiT K1 RCPU (Nuclei N308, RV32) on MUSE Pi Pro. Use when building the RCPU firmware via build.sh, flashing through J-Link/OpenOCD + GDB, monitoring the r_uart0 serial console, debugging boot faults (mepc/mcause/ECLIC), running a full boot-verify one-shot, or validating via QEMU. Covers the self-built PXA UART driver, DDR remap (entry 0x30000100) for images over 256KB, wiring, and AP-side RCPU enablement. Triggers on 编译openvela、烧录、JTAG、串口、调试、bringup、NSH、RCPU、N308、MUSE Pi Pro、k1-rcpu.
---

# K1 RCPU Bringup: 编译、启动、串口、调试全流程

> 适用：SpacemiT K1 MUSE Pi Pro 的 RCPU（Nuclei N308，RV32 M-Mode）
> 固件：openvela/NuttX，vendor/SpaceMiT/chips/k1-rcpu + boards/k1-rcpu/muse_pi_pro_rcpu（当前为 **DDR remap 布局**）
> 工具：J-Link（JTAG）+ 串口 + Spacemit OpenOCD + riscv-none-elf-gdb
> 状态：**L0 已达成，且 2026-08-22 真机一键验证通过（编译/启动/串口/调试全流程可复现）**

---

## §0 当前规范 canonical —— 先用这个，别用错参数

| 项 | 值 |
|---|---|
| 固件布局 | **DDR remap**。LMA `0x30000000`，ELF entry `0x30000100` |
| 固件产物 | `work/vela-workspace/cmake_out/muse_pi_pro_rcpu_nsh/nuttx`（ELF32）+ `nuttx.bin`（~277KB） |
| RCPU 串口 | **`/dev/ttyACM0`**（ATK-HS-V4-CMSIS-DAP CDC，GPIO47/48 → USB-TTL） |
| 大核控制台 | `/dev/ttyUSB0`（CH340） |
| OpenOCD | GDB `localhost:1024`，telnet `4444`，tcl `6666` |
| JTAG | SEGGER J-Link，SPEED 8000kHz（不稳降 2000），`TARGET rcpu` |

> ⚠️ **`work/dev-loop/env.sh` 和 `bridge.sh` 里仍是旧 SRAM 布局入口 `0x100`（已过时）**。烧录一律用本 skill 的 `0x30000100`，**不要**用它俩的 `ENTRY_ADDR`/`resume 0x100`。

---

## §0.5 能不能全自动？（先做这一条判断）

```bash
id | grep -q dialout && echo "dialout OK"
ls -l /dev/ttyACM0 /dev/ttyUSB0
```

- **两条都成立** → AI 沙箱可全自动（采集串口 + GDB 烧录 + NSH 交互），无需人在环。
- **任一缺失** → 串口部分需宿主机开采集（`cd work/dev-loop && ./rcpu-log.sh start rcpu`）；GDB `localhost:1024` 仍可从沙箱直连。

---

## §1 一键验证 Runbook（最快路径，按顺序执行）

> 这是 2026-08-22 真机验证成功的那一套。已在 `work/dev-loop` 下依次执行。

```bash
cd /mnt/data/openvela_contest/document/work/dev-loop
source env.sh >/dev/null 2>&1   # 复用 FIRMWARE / GDB / 端口（但入口地址以下面 0x30000100 为准）

# ① 起串口采集（后台）
: > log/rcpu.log
stty -F /dev/ttyACM0 115200 raw -echo
( cat /dev/ttyACM0 >> log/rcpu.log 2>&1 ) & echo $! > log/rcpu.pid
echo "capture pid=$(cat log/rcpu.pid)"

# ② 编译（仅源码改动后需要；产物未变可跳过）
cd $VELA_WORK
./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh --cmake -c $CUSTOM_GCC -j8
riscv-none-elf-readelf -h cmake_out/muse_pi_pro_rcpu_nsh/nuttx | grep -iE "Entry|Class"   # 期望 Entry 0x30000100

# ③ 烧录并启动（reset halt → load → resume 0x30000100）
$GDB -batch -nx \
  -ex "set confirm off" -ex "set pagination off" -ex "set height 0" \
  -ex "target remote localhost:$OPENOCD_GDB_PORT" \
  -ex "monitor reset halt" -ex "load" -ex "monitor resume 0x30000100" \
  -ex "monitor poll" -ex "detach" -ex "quit" \
  $FIRMWARE

# ④ 等启动，读日志：期望 B → C → NuttShell (NSH) → nsh>
sleep 5; cat -A log/rcpu.log

# ⑤ 交互验证（证明串口双向可用）
printf 'uname -a\r' > /dev/ttyACM0; sleep 1
printf 'help\r'     > /dev/ttyACM0; sleep 2
tail -50 log/rcpu.log   # 期望 uname 回 NuttX 0.0.0 ... risc-v muse_pi_pro_rcpu；help 回命令表

# ⑥ 清理
kill "$(cat log/rcpu.pid)" 2>/dev/null; rm -f log/rcpu.pid
```

**期望串口输出（判定成功）**：`BC`（`A` 可能因首字节被 CDC 漏掉，忽略）→ `NuttShell (NSH)` → `nsh> `。

---

## §2 环境常量与路径

```bash
DOC_ROOT="/mnt/data/openvela_contest/document"
WORK_DIR="$DOC_ROOT/work"; VELA_WORK="$WORK_DIR/vela-workspace"
CUSTOM_GCC="$VELA_WORK/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc"
GDB="/home/jack/toolchain/riscv-none-elf-gcc/bin/riscv-none-elf-gdb"
OPENOCD_DIR="$WORK_DIR/firmware/openocd/bin"; OPENOCD="$OPENOCD_DIR/openocd"
OPENOCD_SCRIPTS="$OPENOCD_DIR/../share/openocd/scripts"
BOARD_CONFIG="vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh"
BUILD_DIR="$VELA_WORK/cmake_out/muse_pi_pro_rcpu_nsh"
FIRMWARE="$BUILD_DIR/nuttx"; FIRMWARE_BIN="$BUILD_DIR/nuttx.bin"
TTY_AP="/dev/ttyUSB0"; TTY_RCPU="/dev/ttyACM0"
OPENOCD_GDB_PORT=1024; OPENOCD_TELNET_PORT=4444; OPENOCD_SPEED=8000; BAUD=115200
ENTRY_ADDR=0x30000100     # DDR remap 布局入口（重要）
```

---

## §3 编译（build.sh 增量编译）

```bash
cd $VELA_WORK
./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh --cmake -c $CUSTOM_GCC -j8
```

- board_config 必须是真实目录（不能写短名）；build.sh 是增量（cmake+ninja）。
- 产物：`cmake_out/muse_pi_pro_rcpu_nsh/nuttx`（ELF32）、`nuttx.bin`。
- 查看：`riscv-none-elf-size $FIRMWARE`、`readelf -h`（Entry `0x30000100`）。
- 详细编译（FLAT / PROTECTED 两套）/踩坑见 `references/build.md`。

---

## §4 烧录与启动（OpenOCD + GDB）

### 4.1 启动 OpenOCD（一般已在跑，先 `ss -tln | grep 1024`）
```bash
cd $OPENOCD_DIR
./openocd -c "bindto 0.0.0.0" -c "gdb port $OPENOCD_GDB_PORT" -c "telnet port $OPENOCD_TELNET_PORT" \
  -c "set SPEED $OPENOCD_SPEED" -c "set SECJTAG 0" -c "set TARGET rcpu" \
  -f $OPENOCD_SCRIPTS/interface/jlink.cfg -f $OPENOCD_SCRIPTS/spacemit_helper.tcl \
  -f $OPENOCD_DIR/scripts/spacemit-k1.cfg
# 等价：cd $OPENOCD_DIR && ./k1-rcpu.sh
```

成功标志：`target created: k1.cpu_rcpu.0` + `VTarget ≈ 3.32V` + `[k1.rcpu] ... Examination succeed`。

### 4.2 GDB 烧录（batch）—— DDR remap 入口 0x30000100
```bash
$GDB -batch -nx -ex "set confirm off" -ex "set pagination off" -ex "set height 0" \
  -ex "target remote localhost:$OPENOCD_GDB_PORT" \
  -ex "monitor reset halt" \
  -ex "set {unsigned int}0xC08800C0 = 0x100000" \
  -ex "printf \"REMAP=0x%x\\n\", *(unsigned int*)0xC08800C0" \
  -ex "load" \
  -ex "monitor reg pc" -ex "monitor resume $ENTRY_ADDR" \
  -ex "monitor poll" -ex "detach" -ex "quit" $FIRMWARE
```

> ⚠️ **`set {unsigned int}0xC08800C0 = 0x100000` 是必须的前置步骤（2026-09-09 新增，见 issue-log I-46）**：
> `DDR_REMAP_BASE` 决定 RCPU 的 `0x30000000` 窗口映射到哪段物理 DDR，**`monitor reset halt` 不会复位它**
> （AON 域，跨 reset 存活）。若它被任何 GDB 实验写成别的值（如 `0x20000000`），
> 烧录后 CPU 取指会拿到垃圾 → `Illegal instruction` PANIC 或**串口完全无输出**，
> 且看起来像"驱动 bug"，极易误判。烧录脚本一律先写 `0x100000` 再 `load`。
> 排查用：读回该寄存器；A/B 对照（写坏值→无输出；写 `0x100000`→`BC`→`nsh>`）。
> 现成脚本：`work/dev-loop/step1_restore.gdb`（恢复+烧录+启动一步到位）。

> 硬件接线、前置条件、devmem 兜底、DDR remap、详细排查见 `references/debug.md`。

---

## §5 串口监控

- 交互：`picocom -b 115200 /dev/ttyACM0` 或 `minicom -D /dev/ttyACM0 -b 115200`
- 自动化采集：`cat /dev/ttyACM0 >> log/rcpu.log 2>&1 &`
- 接线：Pin8=GPIO47 r_uart0_tx→USB-TTL RX；Pin9 GND；Pin10=GPIO48 r_uart0_rx→USB-TTL TX
- 波特率 115200 8N1；UART 基址 `0xC0881000`(SHUB_UART0)；时钟 pll1_aud_24p5=24.576MHz

---

## §6 调试与故障定位（2026-08-22 已验证）

### 6.1 开 GDB 交互
```bash
$GDB $FIRMWARE
(gdb) set pagination off
(gdb) target remote :1024
(gdb) monitor reset halt
(gdb) load
(gdb) break __k1r_start      ; 芯片层入口符号
(gdb) info reg pc sp ra      ; 读寄存器
(gdb) info registers mepc mcause mtval ; 读 CSR（⚠️ 不要用 p/x $mcause $mepc $mtval，那是非法语法）
(gdb) x/8i $pc               ; 反汇编
(gdb) si                     ; 单步
(gdb) delete breakpoints
(gdb) monitor resume         ; 继续跑（或 continue）
```

### 6.2 崩溃现场定位（batch）
```bash
$GDB -batch -nx \
  -ex "target remote localhost:1024" -ex "info reg pc sp ra" \
  -ex "info registers mepc mcause mtval" -ex "x/8i \$pc" \
  -ex "detach" -ex "quit" $FIRMWARE
```
- 反汇编定位：`riscv-none-elf-objdump -d $FIRMWARE | grep -A10 -B5 <地址>`
- **启动进度字符**：`A`=进入 __k1r_start（清 BSS/搬 data）；`B`=串口就绪；`C`=板级初始化完成，随后 `nsh>`。

> 完整 ECLIC 中断/异常 dump 解读、排障表、devmem 寄存器表见 `references/debug.md`。

### 6.3 已发现并修正的坑
- 编译后务必用 `readelf -h` 核对 Entry 是 `0x30000100`（DDR remap）。若显示 `0x100` 说明用的是旧 SRAM 布局链接脚本。
- `p/x $mcause $mepc $mtval` 一次读多 CSR 是 GDB 非法语法，必须逐个 `p/x` 或 `info registers mepc mcause mtval`。

---

## §7 一键闭环脚本（work/dev-loop/，⚠️ 入口已过时）

```bash
cd $DOC_ROOT/work/dev-loop
./rcpu-loop.sh              # 完整一轮：编译→烧录→采集→判断→报告
./rcpu-loop.sh --no-build   # 只烧录+采集
./rcpu-openocd.sh start|stop|status|restart
./rcpu-log.sh start|stop|status|tail
./rcpu-flash.sh [--no-build]
```

> ⚠️ `rcpu-loop.sh/rcpu-flash.sh/bridge.sh` 内部仍用 `ENTRY_ADDR=0x100`（SRAM 布局），**烧当前 DDR-remap 固件会失败**。改代码或直接用 §1 runbook 里的 GDB 命令（`0x30000100`）。已实测：`env.sh`、`bridge.sh` 需同步把入口改 `0x30000100`。

**日志判断分类**（rcpu-loop.sh）：

| 分类 | 触发 | 含义 | 排查方向 |
|---|---|---|---|
| `SUCCESS` | 出现 `nsh>` | NSH 就绪 | — |
| `CRASH` | `PANIC`/`EXCEPTION`/`MCAUSE` | 异常 | mepc/mcause 反汇编定位 |
| `STUCK_NSH` | 见 `C` 无 `nsh>` | 卡 nx_start | appinit/驱动注册/tick |
| `STUCK_BOARD` | 见 `B` 无 `C` | 卡板级 init | k1r_boardinit 外设 |
| `STUCK_EARLY` | 见 `A` 无 `B` | 卡串口 init | head.S mtvec/ECLIC/UART 时钟 |
| `NOISE` | 有输出看不懂 | 乱码 | 波特率/除数/TX-RX 反 |
| `NO_OUTPUT` | 无输出 | 没跑起来 | pinctrl/电源/烧录/接线 |

---

## §8 QEMU 验证（无需真机）

```bash
cd $VELA_WORK; QEMU=/home/jack/toolchain/bin/qemu-system-riscv64
$QEMU -semihosting -M virt,aclint=on -cpu rv64 -smp 1 -bios default -kernel cmake_out/rv-virt_knsh64_xpack/nuttx -nographic
# 8 核 SMP：需 kconfig-tweak 置 SMP_NCPUS=8 与 NCPUS=8；-bios $WORK_DIR/firmware/opensbi-built/v1.9-upstream/firmware/fw_dynamic.bin -smp 8
```
> 完整构建/hostfs/FLAT ostest 见 `references/qemu.md`。

---

## §9 硬件接线速查

### J-Link → 40Pin 排针（JTAG 同名直连，不交叉）
| J-Link | 信号 | 板子 |
|---|---|---|
| Pin5 | TDI | **Pin7** (PRI_TDI) |
| Pin7 | TMS | **Pin11** (PRI_TMS) |
| Pin9 | TCK | **Pin13** (PRI_TCK) |
| Pin13 | TDO | **Pin15** (PRI_TDO) |
| GND | — | 任一 GND(6/9/14/25/30/34/39) |
| Pin1 | VTref | 3V3(Pin1 或 17) |
| Pin3/15 | nTRST/nRESET | 不接 |

### RCPU 串口（见 §5）

---

## §10 AP 侧使能 RCPU

```bash
# A. 自动：官方 dts 默认使能 &rcpu + &r_uart0，启动即可
cat /sys/kernel/debug/pm_genpd/pm_genpd_summary | grep -A1 SPT_PD_AUDIO
cat /sys/class/remoteproc/remoteproc*/state          # running
echo stopped > /sys/class/remoteproc/remoteproc0/state  # 停官方 esOS 避免与 JTAG 冲突
# B. devmem 兜底
devmem 0xC08800D8 32 0x17    # UART0 时钟+复位
devmem 0xC08800C0 32 0x100000 # DDR_REMAP_BASE（DDR 布局必须）
devmem 0xC088C030 32 1        # MCU 运行控制
```

---

## §11 已知问题速查

| Issue | 问题 | 状态 |
|---|---|---|
| I-20 | mtvec 漏写 csrw → 异常跳 0x0 | ✅ |
| I-23 | mhartid 判定 | ✅ |
| I-24 | data 搬移覆盖 | ✅ |
| I-26 | UUE 位 bit7→bit6 | ✅ |
| I-28 | fclk 时钟源 + pll1 gate | ✅ |
| I-30 | 中断/异常未区分 | ✅ |
| I-36/37 | ~~N308 取指 bug → 禁 C 扩展~~（I-42 纠正：**误判**，C 可安全启用） | ✅ C 已启用 |
| I-38 | 🎉 L0 达成 | ✅ |
| I-39 | cliccfg nlbits | ✅ |
| I-41 | 自研 PXA UART，NSH 可交互 | ✅ |

---

## 详细参考资料

| 文件 | 内容 |
|---|---|
| `references/build.md` | 编译命令（FLAT / PROTECTED）、产物体积、历史踩坑、工具链、DDR remap |
| `references/debug.md` | 接线、前置、OpenOCD/GDB 加载、串口验收、排障表、devmem 表 |
| `references/qemu.md` | QEMU（knsh64/ksmp64/flat ostest） |

> 背景：`knowledge/porting/` 下 `k1-rcpu-porting-analysis.md`、`k1-rcpu-pxa-uart-driver.md`、`k1-rcpu-n308-fetch-bug-temp-fix.md`、`openvela-issue-log.md`、`k1-rcpu-dev-loop.md`。
