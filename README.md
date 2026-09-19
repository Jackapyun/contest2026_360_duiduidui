# MUSE Pi Pro（SpacemiT K1）RCPU 实时核 openvela 适配

> 2026 首届 openvela AI 硬件开发者大赛 · **新硬件适配赛道** · 队伍/专属仓：`contest2026_360_duiduidui`

---

## 一、作品简介

本作品把 **openvela（基于 Apache NuttX）** 移植到 **MUSE Pi Pro（SpacemiT K1）** 的 **RCPU 实时核**——一颗 **Nuclei N308（RV32, M-Mode, 无 MMU）** 的音频/实时协处理器上，并从零完成 bringup 与系统级验证（串口控制台、系统 tick、用户态测试、内存隔离）。

K1 是一颗「8×X60 大核 + RCPU 小核」的异构 SoC：大核跑 Linux（Bianbu），RCPU 负责音频与低功耗实时任务。openvela 此前未适配该板，官方「待适配开发板清单」中 MUSE Pi Pro 列第 7 项。我们把 RCPU 作为独立目标核从零 bringup，而不是复用大核的 S-Mode 路径，原因是：**RCPU 无 MMU、无 S-Mode、只有 256KB SRAM + PMP**，形态与常规 RISC-V 移植完全不同，技术难度高、且能验证 openvela 在「异构小核」上的可用性。

**已达成（全部真机验证，非仅编译）**：

- **L0 最小系统**：串口 `nsh>` 交互（`uname`/`help`/`ls`/`echo` 全响应）
- 自研 **PXA 变体 UART 驱动**（替代 ns16550，修正 `RTOIE/UUE/BUS32` 语义，零丢字符）
- 系统 **tick + `ostest` 全量跑通**
- 启用 **C 压缩扩展**（text 276KB → 209KB）
- 内核启动时**自配置 RCPU 核心时钟**（不依赖大核遗留状态）
- **PROTECTED 构建**：无 MMU 条件下用 **PMP 实现内核/用户态隔离**（3 条 TOR 全生效：内核 M-only / 用户 RWX / 兜底 deny），
  NSH 实测运行在 **U-mode**，命令经 **ECALLU** 陷入内核（断点实测 `mepc=0x3008ae06` 落在用户镜像区、`mcause=8`）

固件体积：FLAT `text = 208,740 B`（`nuttx.bin` 209,940 B）；PROTECTED 内核 119,704 B + 用户镜像 118,704 B。
均为 **DDR remap 布局**（`entry 0x30000100`，2MB 预算内余量充足）。

> 说明：本次交付范围 = **RCPU bringup + 系统验证**。曾探索的 RCPU↔AP 多核 rpmsg 传输**未达到端到端可用**
> （大核侧始终未建立 `rpmsg-syslog` 通道），故**整体不纳入本次提交**，相关代码已从本仓移除；
> 其过程记录保留在 AI 日志与知识库 issue-log（I-47/I-49）中。

---

## 二、选题方向

**新硬件适配**（BSP 移植 + 基础外设驱动 + 系统运行）。

选择理由：openvela 的价值在于「一套 OS 覆盖大/小核异构硬件」。K1 的 RCPU 是典型的「无 MMU 实时小核」，现有 openvela 移植样例（c906/sg2000/jh7110）都带 MMU/S-Mode，无法直接套用。把这块啃下来，既补齐 MUSE Pi Pro 的适配空白，也沉淀出一套「无 MMU RISC-V 小核移植 + PMP 隔离」的可复用方法。

---

## 三、目录结构

```text
contest2026_360_duiduidui/
├── board/contest_board/            # ★ 板级适配（本作品主体）
│   ├── apply.sh                    # 把适配源码安装进 openvela 工作区（幂等）
│   └── k1-rcpu/
│       ├── chips/k1-rcpu/          # 芯片层：启动/ECLIC/时钟/UART/PMP
│       ├── boards/k1-rcpu/muse_pi_pro_rcpu/   # 板级：defconfig(×2: FLAT/PROTECTED)、board.h、链接脚本、kernel/
│       ├── nuttx-patches/          # 需同步到公共 nuttx 仓的补丁（2 个）
│       └── docs/adaptation-guide.md           # ★ 完整适配指南（构建/烧录/验证/原理）
├── skills/k1-bringup-build-debug/  # AI Skill：编译/烧录/串口/调试一键 runbook
├── logs/<github_login>/            # AI Coding 对话日志（JSONL）
└── README.md                       # 本文件
```

> 说明：本仓 manifest `contest2026_360_duiduidui.xml` 已按官方约定声明 `vendor/SpaceMiT`
> 并用 `<linkfile>` 把 `board/contest_board/k1-rcpu/` 软链到 `vendor/SpaceMiT/{chips,boards}`
> —— **`repo sync` 后即可直接编译，无需手工 copy，生产仓库零改动**（这也是官方要求的映射方式）。
> `board/contest_board/apply.sh` 仅作为「非 repo 工作区 / 开发机快捷同步」的备用手段保留。

---

## 四、运行方式

### 4.1 拉取工程

```bash
repo init -u https://github.com/open-vela/contest2026_360_duiduidui \
  -b dev-ai-contest-2026 -m contest2026_360_duiduidui.xml
repo sync -c -j8
```

同步完成后，本仓适配代码已由 manifest 软链到位：

```text
vendor/SpaceMiT/chips/k1-rcpu                     -> contest2026_360_duiduidui/board/contest_board/k1-rcpu/chips/k1-rcpu
vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu   -> contest2026_360_duiduidui/board/contest_board/k1-rcpu/boards/k1-rcpu/muse_pi_pro_rcpu
```

### 4.2 编译

```bash
# ① （可选）应用公共 nuttx 仓补丁——本适配依赖 2 个 nuttx 改动，
#    正式流程应通过 fork + PR 提交到 open-vela/nuttx 的 dev-ai-contest-2026 分支；
#    在补丁尚未合入前，可用下面这条命令在本地打上：
cd contest2026_360_duiduidui/board/contest_board && ./apply.sh --patches && cd ../../..

# ② 编译（在 openvela 工作区根目录）
#    不需要 ccache：本适配已移除 build.sh 里的 ccache compiler launcher，
#    装不装 ccache 构建产物一致（体积可复现，见 4.5）。
./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh \
    --cmake -c $PWD/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc -j8

# ③ 核对产物（期望 ELF32、entry 0x30000100）
riscv-none-elf-readelf -h cmake_out/muse_pi_pro_rcpu_nsh/nuttx | grep -iE "Entry|Class"
```

> 补丁清单见 `board/contest_board/k1-rcpu/nuttx-patches/`（ECLIC mcause 高位屏蔽、build.sh 自定义工具链）。
> 若你的工作区不是用 `repo init/sync` 拉的，再用 `./apply.sh` 手工同步源码。

本适配提供**两套构建模式**，均已真机验证：

| 配置 | 模式 | 产物 | 入口 | 真机验证 |
|---|---|---|---|---|
| `configs/nsh` | **FLAT**（交付基线） | `nuttx.bin` 209,940 B | `0x30000100` | `ostest` 31 项全过 |
| `configs/knsh` | **PROTECTED**（进阶能力） | `nuttx` 119,704 B + `nuttx_user` 118,704 B | 内核 `0x30000100` / 用户 `0x30080000` | `ostest` 28 项全过 + PMP/ECALLU 取证 |

- **FLAT** 是赛道要求的 L0 基线：功能最全（含 `ostest` / `mm` / `getprime` / `hello`），NSH 运行在 **M-mode**；
- **PROTECTED**（`CONFIG_BUILD_PROTECTED=y` + `CONFIG_ARCH_USE_MPU=y`）为 **2-pass 构建**，用 PMP 三条 TOR 做内核/用户隔离，
  NSH 运行在 **U-mode**，系统调用经 **ECALLU** 陷入内核。
  ⚠️ **内核 ELF 不内嵌用户镜像，烧录时必须分别 `load` 两个文件**（见 4.3）。
- 两套模式的 ostest 项数差异（PROTECTED 少 `FPU` / `spinlock` / `wdog` 三项）是上游设计使然：这三项在
  `apps/testing/ostest/ostest_main.c` 中由 `#ifdef CONFIG_BUILD_FLAT` 包裹，PROTECTED 下本就不编译，**非失败**。

### 4.3 烧录与运行（J-Link + OpenOCD）

```bash
# OpenOCD（TARGET=rcpu，GDB 端口 1024）
cd work/firmware/openocd/bin && ./openocd -c "bindto 0.0.0.0" -c "gdb port 1024" \
  -c "telnet port 4444" -c "set SPEED 8000" -c "set SECJTAG 0" -c "set TARGET rcpu" \
  -f ../share/openocd/scripts/interface/jlink.cfg \
  -f ../share/openocd/scripts/spacemit_helper.tcl -f scripts/spacemit-k1.cfg

# 串口（RCPU r_uart0 = /dev/ttyACM0，115200 8N1）
stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0

# 烧录并启动 —— FLAT（configs/nsh，单个镜像）
# ★ 必须先写 DDR_REMAP_BASE=0x100000，否则取指跑飞/无输出
riscv-none-elf-gdb -batch -nx \
  -ex "target remote localhost:1024" -ex "monitor reset halt" \
  -ex "set {unsigned int}0xC08800C0 = 0x100000" -ex "load" \
  -ex "monitor resume 0x30000100" -ex "detach" -ex "quit" \
  cmake_out/muse_pi_pro_rcpu_nsh/nuttx
```

**PROTECTED（`configs/knsh`）必须烧两个文件**——内核 ELF 不内嵌用户镜像：

```bash
riscv-none-elf-gdb -batch -nx \
  -ex "target remote localhost:1024" -ex "monitor reset halt" \
  -ex "set {unsigned int}0xC08800C0 = 0x100000" \
  -ex "load cmake_out/muse_pi_pro_rcpu_knsh/nuttx" \
  -ex "load cmake_out/muse_pi_pro_rcpu_knsh/nuttx_user" \
  -ex "monitor resume 0x30000100" -ex "detach" -ex "quit"
```

### 4.4 复现验收（串口出现 `nsh>` 后）

```text
uname -a        → NuttX 0.0.0 ... risc-v muse_pi_pro_rcpu
ls /dev         → console null ttyS0 zero
ostest          → 全部子测试通过，Exiting with status 0
```

启动日志关键行：

```text
BC            ← FLAT
BCD           ← PROTECTED（多一个 D）
NuttShell (NSH)
nsh>
```

更详细的原理、排障、寄存器级说明见 **`board/contest_board/k1-rcpu/docs/adaptation-guide.md`**。

### 4.5 真机验证证据（2026-09-19，两套模式各一份）

完整的 GDB 烧录记录、串口原始日志、PMP/特权级取证与自动校验清单：

| 模式 | 证据目录 | 关键结论 |
|---|---|---|
| FLAT | `docs/evidence/2026-09-19-flat-nsh-ostest/` | `BC`→`nsh>`；`ostest` 31 项全过、`status 0` |
| PROTECTED | `docs/evidence/2026-09-19-protected-knsh/` | `BCD`→`nsh>`；`pmpcfg0=0x80f08` 三条 TOR；`mepc=0x3008ae06`+`mcause=8` 证明 U-mode；`ostest` 28 项全过、`status 0` |

> 每份目录内含：合并版主 log、`manifest.txt`（源码 commit / 镜像 md5 / 入口）、
> `gdb-flash-and-load.log`（烧录 transcript）、`rcpu-serial-console.log`（原始串口）与 `-clean.log`（可读版）；
> PROTECTED 另有 `gdb-pmp-forensics.log`（PMP 寄存器）与 `gdb-syscall-proof.log`（ECALLU 实证）。
> 注：镜像内嵌编译时刻，**体积可复现、md5 不可跨次复现**。

---

## 五、AI Coding 使用说明

本作品全程使用 AI 编程助手协作开发（工具：**DeepSeek Harness / DSH**，会话日志已导出到 `logs/`）。

| 环节 | AI 协作方式 |
|---|---|
| 资料消化 | 从 K1 用户手册（17 章）、datasheet、esOS 源码、Zephyr N308 dts、Linux 侧 remoteproc/mailbox 驱动中提取寄存器语义与协议约定，沉淀成结构化知识库 |
| 方案设计 | 对比多种技术路线（S-Mode/OpenSBI、双世界 TEE、AHBDMA vs 专用 ADMA），用实测证据否定错误方案 |
| 编码 | 芯片层/板级/UART/PMP 驱动实现，Kconfig/CMake/defconfig 集成 |
| 调试 | 用 GDB + 串口日志做**根因定位**：ECLIC 异常上下文、PMP TOR 对齐、系统 tick 定时器未武装、PXA UART 的 RDA 依赖 RTOIE 等，均定位到具体代码/寄存器 |
| 文档与 Skill | 产出适配指南与可复用 Skill（`skills/k1-bringup-build-debug/`：编译→烧录→串口→调试一键 runbook） |

**AI 带来的实际收益**：把「手册 → 寄存器 → 驱动代码 → 真机验证」的闭环从数天压缩到小时级；尤其在**无 MMU 的 PMP 隔离**与**异构小核中断/时钟根因定位**上，AI 帮助快速比对了多个参考实现并锁定了极易误判的坑（详见适配指南 §6）。

对话日志（JSONL，含工具调用与结果）见 `logs/`。
