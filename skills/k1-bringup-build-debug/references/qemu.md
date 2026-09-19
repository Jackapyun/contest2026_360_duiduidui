# QEMU 验证（无需真机）

用于在无真机的环境下验证内核/并行度/NSH；RCPU 真机形态与 QEMU 的 S-Mode + OpenSBI 布局一致。

## 环境

```bash
cd $VELA_WORK
QEMU=/home/jack/toolchain/bin/qemu-system-riscv64
# openvela prebuilt 工具链（rv64, 用于 S-Mode 验证）
XPACK_TC="$VELA_WORK/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc"
```

## 单核 knsh64（S-Mode + OpenSBI）

```bash
cmake -B cmake_out/rv-virt_knsh64_xpack -S nuttx \
  -DBOARD_CONFIG=rv-virt:knsh64 \
  -DCUSTOM_MODULE_PATH=$PWD/build/cmake \
  -DCMAKE_C_COMPILER=$XPACK_TC \
  -DCMAKE_ASM_COMPILER=$XPACK_TC \
  -GNinja
ninja -C cmake_out/rv-virt_knsh64_xpack -j8

# hostfs 部署（把 init/sh/hello 放入 apps/bin, NSH 能跑 hostfs 程序）
mkdir -p cmake_out/apps/bin
cp cmake_out/rv-virt_knsh64_xpack/bin/{init,sh,hello} cmake_out/apps/bin/

# 启动
$QEMU -semihosting -M virt,aclint=on -cpu rv64 -smp 1 \
  -bios default -kernel cmake_out/rv-virt_knsh64_xpack/nuttx -nographic
```

## 8 核 SMP ksmp64

```bash
# 关键：NCPUS 与 SMP_NCPUS 必须一致，否则 SMP 启动异常
kconfig-tweak --file cmake_out/rv-virt_ksmp64_xpack/.config --set-val SMP_NCPUS 8
kconfig-tweak --file cmake_out/rv-virt_ksmp64_xpack/.config --set-val NCPUS 8

$QEMU -semihosting -M virt,aclint=on -cpu rv64 -smp 8 \
  -bios $WORK_DIR/firmware/opensbi-built/v1.9-upstream/firmware/fw_dynamic.bin \
  -kernel cmake_out/rv-virt_ksmp64_xpack/nuttx -nographic
```

> ksmp64 完整启动到 NSH 依赖：自定义 fdt 锁 boot=0 + CONFIG_NCPUS=8。
> 相关排障与历史见 `knowledge/porting/qemu-opensbi-smode-verified.md` 与 `knowledge/porting/openvela-issue-log.md` I-06。

## FLAT 模式 ostest 全量验证

```bash
# FLAT: 内核与应用同一地址空间，便于全量测试（ostest）
ninja -C cmake_out/rv-virt_flatsmp64_xpack -j8
$QEMU -semihosting -M virt,aclint=on -cpu rv64 -smp 8 \
  -bios $WORK_DIR/firmware/opensbi-built/v1.9-upstream/firmware/fw_dynamic.bin \
  -kernel cmake_out/rv-virt_flatsmp64_xpack/nuttx -nographic
# NSH 内执行: ostest → 应输出 "Exiting with status 0"
```

## 相关文档

| 文档 | 内容 |
|---|---|
| `knowledge/porting/qemu-opensbi-commands.md` | QEMU 验证命令速查 |
| `knowledge/porting/qemu-opensbi-smode-verified.md` | S-Mode/OpenSBI 验证成功路径 |
| `knowledge/porting/qemu-smode-nsbi-failure.md` | NuttSBI 缺陷记录 |
| `knowledge/porting/toolchain-qemu-bringup-verified.md` | 工具链 medlow 问题与复现，QEMU bringup 验证 |
