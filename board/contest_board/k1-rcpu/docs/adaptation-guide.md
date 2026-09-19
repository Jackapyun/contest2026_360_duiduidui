# MUSE Pi Pro / SpacemiT K1 RCPU（Nuclei N308）openvela 适配指南

> 作者：contest2026_360_duiduidui ｜ 更新：2026-09-09
> 目标板：**MUSE Pi Pro（SpacemiT K1）** ｜ 适配核：**RCPU / Nuclei N308（RV32, M-Mode, 无 MMU）**

---

## 一、成果概览

| 里程碑 | 状态 | 真机证据 |
|---|---|---|
| 芯片层 + 板级骨架（RV32, DDR remap 布局） | ✅ | 编译通过，ELF entry `0x30000100` |
| **L0：串口 NSH 交互** | ✅ | `nsh>` + `uname`/`help`/`ls`/`echo` 全响应 |
| 自研 PXA 变体 UART 驱动 | ✅ | 无丢字符，`RTOIE/UUE/BUS32` 语义修正 |
| C 压缩扩展启用 | ✅ | text 276KB → 209KB |
| 系统 tick + `ostest` 全量跑通 | ✅ | `ostest` 无失败项 |
| 内核自配置 RCPU 核心时钟 | ✅ | 启动时写 `AUDIO_CLK_RES_CTRL`，245.7MHz |
| **PROTECTED 构建（PMP 内核/用户隔离）** | ✅ | `pmpcfg0=0x80f08` 3 条 TOR 生效，用户态 `MPP=0` |

> 固件体积：text ≈ 210KB（210,764 B；DDR 布局，预算 2MB，余量充足）。
>
> **交付范围**：RCPU bringup + 系统验证。曾探索的 RCPU↔AP 多核 rpmsg 传输未达端到端可用（大核侧未建立通道），
> **不纳入本次提交**，代码已从本仓移除；过程见知识库 issue-log I-47/I-49。

---

## 二、代码结构

```
board/contest_board/
├── apply.sh                    # 把本目录源码安装进 openvela 工作区
└── k1-rcpu/
    ├── chips/k1-rcpu/          # 芯片层（→ vendor/SpaceMiT/chips/k1-rcpu）
    │   ├── k1r_head.S          # 启动入口、清 BSS/搬 data、mtvec、开 I/D Cache
    │   ├── k1r_start.c         # 时钟配置、板级早期初始化
    │   ├── k1r_irq.c           # ECLIC 中断控制器 + irq_attach/enable
    │   ├── k1r_irq_dispatch.c  # 中断/异常分派（区分 IRQ 与 exception）
    │   ├── k1r_timerisr.c      # 系统 tick（N308 SYSTIMER/MTIME）
    │   ├── k1r_lowputc.c       # 早期串口输出
    │   ├── k1r_serial.c        # PXA 变体 UART 驱动（console）
    │   ├── k1r_allocateheap.c  # DDR 堆
    │   ├── k1r_userspace.c     # PROTECTED 的 PMP 配置（3 条 TOR）
    │   ├── hardware/*.h        # 寄存器定义（memorymap/ccu/uart/eclic/timer）
    │   └── include/            # chip.h / irq.h
    ├── boards/k1-rcpu/muse_pi_pro_rcpu/
    │   ├── configs/nsh/        # 主配置（NSH + ostest）
    │   ├── configs/knsh/       # PROTECTED 配置（内核/用户隔离 + 2-pass）
    │   ├── configs/nsh-min/    # 最小体积配置
    │   ├── src/k1r_boardinit.c # 板级初始化（串口由 nx_start 完成）
    │   ├── scripts/*.ld        # DDR remap 链接脚本（LMA 0x30000000）
    │   └── kernel/k1r_userspace.c
    ├── nuttx-patches/          # 需同步到公共 nuttx 仓的补丁
    └── docs/adaptation-guide.md
```

---

## 三、编译

```bash
# 0) 先用官方 manifest 拉取工程——本仓 manifest 已用 <linkfile> 把适配代码
#    软链到 vendor/SpaceMiT/{chips,boards}，sync 后即可直接编译（无需手工 copy）
repo init -u https://github.com/open-vela/contest2026_360_duiduidui \
  -b dev-ai-contest-2026 -m contest2026_360_duiduidui.xml
repo sync -c -j8

# 1) 应用 2 个公共 nuttx 仓补丁（正式流程应 fork + PR 至 open-vela/nuttx
#    的 dev-ai-contest-2026 分支；未合入前可本地先打）
cd contest2026_360_duiduidui/board/contest_board && ./apply.sh --patches

# 2) 编译（在 openvela 工作区根目录）
cd ../../..
export CCACHE_DIR=$PWD/../.ccache
./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh \
    --cmake -c $PWD/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc -j8

# 3) 核对产物
riscv-none-elf-readelf -h cmake_out/muse_pi_pro_rcpu_nsh/nuttx | grep -iE "Entry|Class"
#   期望：Entry point address 0x30000100（DDR remap 布局），Class ELF32
```

> **工作区/映射**：板级代码的**权威副本**在本仓 `board/contest_board/k1-rcpu/`，
> 由 manifest `<linkfile>` 映射到 `vendor/SpaceMiT/chips/k1-rcpu` 与
> `vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu`（生产仓库零改动）。
> 若工作区不是用 `repo sync` 拉的（例如开发机上的旧工作区），可用
> `board/contest_board/apply.sh` 手工同步源码（`cp -a` + 刷新 mtime + 清理构建缓存）。

> **配置改动提醒**：NuttX CMake 只在 `.config` 不存在或 board config 路径变化时才按 `defconfig`
> 重新生成配置。**改了 `defconfig` 后必须删除 `cmake_out/muse_pi_pro_rcpu_nsh/.config`**（或整个
> 构建目录）才会生效，否则改动会被静默忽略。同理，链接脚本会被预处理成
> `cmake_out/<cfg>/ld.script.tmp`，**改 `scripts/ld.script` 后也要删掉该 tmp 文件**，
> 否则链接仍用旧布局（表现为 ELF 入口地址错误）。

---

## 四、烧录与启动（J-Link + OpenOCD + GDB）

```bash
# 1) OpenOCD（TARGET=rcpu）
cd work/firmware/openocd/bin && ./openocd -c "bindto 0.0.0.0" -c "gdb port 1024" \
  -c "telnet port 4444" -c "set SPEED 8000" -c "set SECJTAG 0" -c "set TARGET rcpu" \
  -f ../share/openocd/scripts/interface/jlink.cfg \
  -f ../share/openocd/scripts/spacemit_helper.tcl -f scripts/spacemit-k1.cfg

# 2) 串口采集（RCPU r_uart0 = /dev/ttyACM0）
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0 > log/rcpu.log &

# 3) GDB 烧录并启动（★ 必须先写 DDR_REMAP_BASE）
riscv-none-elf-gdb -batch -nx \
  -ex "target remote localhost:1024" -ex "monitor reset halt" \
  -ex "set {unsigned int}0xC08800C0 = 0x100000" \
  -ex "load" -ex "monitor resume 0x30000100" -ex "detach" -ex "quit" \
  cmake_out/muse_pi_pro_rcpu_nsh/nuttx
```

> ### ⚠️ 关键坑：`DDR_REMAP_BASE`（`0xC08800C0`）
> 它决定 RCPU 的 `0x30000000` 窗口映射到哪段物理 DDR，**且 `monitor reset halt` 不会复位它**
> （AON 域寄存器，跨 reset 存活）。若被写成别的值（例如 GDB 实验残留的 `0x20000000`），
> 烧录后 CPU 取指会拿到垃圾 → `Illegal instruction` PANIC 或**串口完全无输出**，
> 现象极像"驱动 bug"。**烧录脚本一律先写 `0x100000`（AP dts `ddr-remap-base` 权威值）再 load。**

**成功判据**：串口出现 `BC` → `NuttShell (NSH)` → `nsh>`。

---

## 五、验证清单（可复现）

```bash
# NSH 内
uname -a            # → NuttX 0.0.0 ... risc-v muse_pi_pro_rcpu
ls /dev             # → console null ttyS0 zero
uptime              # → 时间正常累加（tick 已启动）
sleep 2             # → 正常返回
ostest              # → 全部子测试通过，Exiting with status 0
```

启动日志（nsh 配置）关键行：

```
BC
NuttShell (NSH)
nsh>
```

GDB 侧取证（可选）：

```
x/1xw 0xd428294c      # RCPU 核心时钟寄存器：低 7 位 = MUX，0 → pll1_aud 245.7MHz
```

PROTECTED（knsh 配置）侧取证：

```
info registers pmpcfg0   # → 0x80f08（3 条 TOR 全生效）
# 用户态运行时读 mstatus.MPP → 0
```

---

## 六、关键技术点

### 6.1 启动与内存布局
- RCPU 视角 DDR = `0x30000000`（256MB），由 `DDR_REMAP_BASE=0x100000` 映射到物理 `0x100000`。
- 固件 LMA `0x30000000`，ELF entry `0x30000100`；保留区布局与 AP dts（`k1-x.dtsi`）一致。

### 6.2 ECLIC 中断与异常
- 中断控制器是 **ECLIC（`0xE0020000`）**，不是 PLIC；`cliccfg.nlbits` 必须配置正确，否则中断不触发。
- `riscv_exception_common.S` 需屏蔽 `mcause` 高位（N308 会在高位塞入额外信息），否则 ECALL 判定不中 —— 见 `nuttx-patches/`。

### 6.3 PXA 变体 UART（`k1r_serial.c`）
- SHUB_UART0 `0xC0881000`，与标准 ns16550 语义不同：`IER.RTOIE`（位错位）、`RTO` 独立处理、`FCR.BUS32`、TX 半空中断。
- 用 vendor 层专用驱动替代 `uart_16550.c`，解决丢字符/不能响应问题。

### 6.4 PROTECTED（无 MMU 的内核/用户隔离）
- `CONFIG_BUILD_PROTECTED` + `ARCH_USE_MPU` + `LIB_SYSCALL` + `BUILD_2PASS`，用 **PMP** 代替 MMU。
- `configure_mpu()` 配置 **3 条 TOR**：内核段（M-only）/ 用户段（U RWX）/ 高位兜底（U deny）。
  - 兜底区**不能加 `L` 位**（`L=1` 对 M 态也生效 → 内核访问高位外设会被锁死）。
  - TOR 边界必须 4 字节对齐，且必须检查 `riscv_config_pmp_region()` 返回值（否则静默失败）。
  - N308 的 `PMPCFG` A 字段在 **bits[4:3]**（非标准 bits[3:2]）。

### 6.5 本次未纳入：RCPU↔AP 多核 rpmsg（`k1_rptun.c`）

探索过程中已实现的 RCPU 侧 rptun 传输（自建资源表/vring、邮箱 kick、`/dev/rptun/vdev0` + `/dev/rpmsg/rcpu` 节点）
**未达到端到端可用**：大核侧始终未建立 `rpmsg-syslog` 通道，且后续排查暴露出 RCPU 侧邮箱清位语义与
AP/esOS 驱动不一致（会引发 ECLIC 电平中断风暴）。因此**该部分代码已从本仓移除，不参与本次交付**。

相关结论与协议细节保留在知识库：`knowledge/porting/k1-rcpu-rpmsg-protocol.md`、issue-log **I-47 / I-49**。

---

## 七、已知限制与后续

| 项 | 说明 |
|---|---|
| RCPU↔AP 多核 rpmsg | **本次未交付**（端到端未打通，代码已移除）。后续需：固件提供 `.resource_table` 段并把入口对齐 AP dts 的 `esos-entry-point=0x30300114`；修正邮箱 IRQ 编号（NuttX IRQ = 16 + ECLIC 30 = 46，而非 30）与邮箱清位语义 |
| AHBDMA | **RCPU 上不可用**：DMA 引擎只寻址 `0x40000000` 窗口，与 CPU 视角 `0x30000000` 不是同一物理；esOS 在 N308 dts 中也把 `pdma@c0884000` 标 `disabled`。数据通路需走专用 ADMA |
| I2C/SPI/GPIO/PWM | 寄存器与时钟已梳理（见知识库），驱动待实现 |
| 硬件 TEE | N308 实测只有 M+U 两级，无 S-Mode/无 S-PMP/无 MMU（`misa=0x4010912f`，`csrr medeleg` 非法） |
| 音频（I2S/ADMA/codec） | 寄存器/时钟/基址已梳理（见知识库），驱动待实现 |

---

## 八、参考

- 芯片手册：K1 用户手册 §6 地址映射、§7 中断、§14 RCPU 子系统
- AP 侧权威：`linux-6.6/drivers/remoteproc/spacemit/k1x-rproc.c`、`drivers/mailbox/spacemit/k1x-mailbox.c`、`arch/riscv/boot/dts/spacemit/k1-x.dtsi`
- 参考实现：NuttX `rv32m1`（32 位 M-Mode 芯片层模板）、`arch/risc-v/src/common`（ECLIC/异常分派）
- 本仓 `../skills/k1-bringup-build-debug/`：编译/烧录/串口/调试一键 runbook
