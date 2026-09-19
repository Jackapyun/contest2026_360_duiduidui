# 编译 RCPU 固件（vendor/SpaceMiT）

## 目录

- 完整命令
- 产物位置与体积
- 关键坑（历史踩坑）
- 工具链说明

## 完整命令

```bash
cd $VELA_WORK

# FLAT（交付基线：NSH + ostest/hello/mm/getprime + 调试信息）
./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh \
  --cmake -c $GCC -j8

# PROTECTED（内核/用户隔离，2-pass，产出 nuttx + nuttx_user 两个镜像）
./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/knsh \
  --cmake -c $GCC -j8
```

清理后重编：在命令末尾加 `distclean`（`build.sh <board_config> --cmake -c $GCC -j8 distclean`），或删除 `cmake_out/muse_pi_pro_rcpu_*` 目录。

## 产物位置与体积

| 配置 | 构建目录 | 产物 | 说明 |
|---|---|---|---|
| nsh（FLAT） | `cmake_out/muse_pi_pro_rcpu_nsh/` | `nuttx.bin` 209,940 B（text 208,740 B） | 全功能；DDR remap 2MB 窗口，余量充足 |
| knsh（PROTECTED） | `cmake_out/muse_pi_pro_rcpu_knsh/` | `nuttx.bin` 119,704 B + `nuttx_user.bin` 118,704 B | 2-pass；用户镜像 LMA `0x30080000` |

关键文件：

- `nuttx` — ELF32，**entry `0x30000100`，DDR remap `0x30000000` 布局**（JTAG `load` 用这个）
- `nuttx.bin` — 原始二进制（判体积 / 后续量产打包用）
- **PROTECTED 专属**：`nuttx_user` / `nuttx_user.bin` — 用户镜像，**内核 ELF 不内嵌它**，烧录时必须在内核之后**再 `load` 一次**（见 `debug.md`）

> 历史上曾有服务于 256KB SRAM 预算的 `configs/nsh-min`（约 103KB）。改为 DDR remap 布局后
> 该前提已不成立且未做真机验证，**已删除**，不要再用。

体积详细构成与进一步裁剪方向见知识库 `knowledge/porting/k1-rcpu-codesize-analysis.md`。

## 关键坑（历史踩坑，务必遵守）

1. **必须用 `build.sh`，不能直接 cmake**。openvela 的 vendor/envsetup 环境与路径约定只有 `build.sh` 会完整处理；直接 cmake 会缺 vendor 环境、defconfig 定位失败。
2. **`board_config` 必须是 vendor 板级 `configs` 目录的完整路径**（`vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh`），不能写 `rcpu_nsh` 之类的短名，否则 CMake 找不到 defconfig。
3. **`head.S` 的 `csrci` 立即数超限**：RCPU 自定义 CSR 地址较大，`csrci` 立即数超 12 位会汇编失败。改法：`li` 加载 CSR 编号到通用寄存器后 `csrc/csrs`。
4. **Kconfig 需 `select ONESHOT_COUNT`**：`riscv_mtimer_initialize` 走新 clkcnt 接口，缺该配置会链接失败。
5. **芯片层必须实现 `up_irq_enable`**：NuttX common 依赖它，vendor 芯片层缺实现会编译/链接报错。
6. **工具链用 openvela prebuilt 的 riscv-none-elf-gcc 13.4.0**：RCPU 是 RV32、地址从 `0x0` 起，不受 I-01 的 medlow libgcc 问题影响（该问题仅影响 rv64 高地址 0x80000000+）。

## 工具链说明

- 编译：`$VELA_WORK/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc`（GCC 13.4.0，支持 `-march=rv32imafdc -mabi=ilp32d`）。
- ISA 建议用 `rv32imafdc`（工具链原生支持）；P/B 扩展暂不启用。
- 不要用厂商（Spacemit）工具链 —— openvela 自带工具链已足够，且避免额外依赖。

## 镜像超过 256KB 时（DDR remap 布局）

SRAM 只有 256KB。若 `nuttx.bin` 超限（当前 nsh ≈ 277KB，即处此情形），转 DDR remap 布局：

1. 链接地址改为 RCPU 视角 **`0x30000000` 附近，entry `0x30000100`**（2026-08-22 实测）。
2. AP 侧先写 `DDR_REMAP_BASE=0x100000`（寄存器 `0xC08800C0`）。
3. 此时 JTAG `load` 目标地址变为 `0x30000000` 段，而非 `0x0` 或官方 esOS 的 `0x30300000`。

> ⚠️ **不要混用**：官方 esOS 是 `0x30300114`/`0x30300000`；本次 openvela build 用的是 `0x30000100`/`0x30000000`。以 `readelf -h` 实际打印为准。

详见 `references/debug.md` §DDR remap 与知识库 `knowledge/porting/k1-rcpu-porting-analysis.md` §三。
