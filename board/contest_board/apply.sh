#!/usr/bin/env bash
#===============================================================================
# apply.sh — 把本仓的 K1 RCPU 适配代码安装到 openvela 工作区
#
# 用法（在专属仓目录内执行）：
#   ./apply.sh              # 安装源码（vendor/SpaceMiT）
#   ./apply.sh --patches    # 同时给 nuttx 打补丁
#   ./apply.sh --dry-run    # 只打印将执行的动作
#
# 背景：本仓 board/contest_board/k1-rcpu/ 是适配代码的**权威副本**；
#       openvela 的编译树需要它在 vendor/SpaceMiT/ 下（board config 路径
#       必须是 vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh）。
#       本脚本把权威副本同步过去，可反复执行（幂等覆盖）。
#===============================================================================
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
K1R="$HERE/k1-rcpu"
DRY=""
PATCHES=""

for a in "$@"; do
  case "$a" in
    --dry-run) DRY="echo [dry-run]" ;;
    --patches) PATCHES=1 ;;
    *) echo "未知参数: $a"; exit 1 ;;
  esac
done

# 定位 openvela 工作区根（本仓的上一级：board/contest_board → board → 本仓 → 工作区）
WS="$(cd "$HERE/../../.." && pwd)"
if [ ! -d "$WS/nuttx" ] || [ ! -d "$WS/vendor" ]; then
  echo "错误：未在 $WS 找到 openvela 工作区（缺 nuttx/ 或 vendor/）"
  echo "请把本仓放在 repo 工作区内再执行。"
  exit 1
fi

TARGET_CHIP="$WS/vendor/SpaceMiT/chips/k1-rcpu"
TARGET_BOARD="$WS/vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu"

echo "==> 工作区: $WS"
echo "==> 芯片层: $TARGET_CHIP"
$DRY cp -a "$K1R/chips/k1-rcpu/." "$TARGET_CHIP/"
echo "==> 板级层: $TARGET_BOARD"
$DRY cp -a "$K1R/boards/k1-rcpu/muse_pi_pro_rcpu/." "$TARGET_BOARD/"

# ★ 刷新 mtime：cp -a 会保留仓内文件的旧时间戳，导致构建树里的派生产物
#   （cmake_out/<cfg>/ld.script.tmp、.config）被 ninja 判定为"不比源文件旧"而
#   不重新生成 —— 后果是改了链接脚本/defconfig 却不生效（曾出现链接脚本仍是上一版
#   布局、入口地址错误的坑）。这里统一 touch 一次，强制下游重新生成。
if [ -z "$DRY" ]; then
  find "$TARGET_CHIP" "$TARGET_BOARD" -type f -exec touch {} +
fi

# ★ 若已有构建目录，删除缓存配置与派生链接脚本（最稳妥是删整个 cmake_out/<cfg>）
for cfgdir in "$WS"/cmake_out/*muse_pi_pro_rcpu*; do
  [ -d "$cfgdir" ] || continue
  if [ -z "$DRY" ]; then
    rm -f "$cfgdir/.config" "$cfgdir/ld.script.tmp"
    [ -f "$cfgdir/include/nuttx/config.h" ] && rm -f "$cfgdir/include/nuttx/config.h"
  fi
  echo "==> 已清理构建缓存: $cfgdir（.config / ld.script.tmp）"
done

if [ -n "$PATCHES" ]; then
  echo "==> 给 nuttx 打补丁"
  for p in "$K1R"/nuttx-patches/*.patch; do
    [ -e "$p" ] || continue
    echo "    $(basename "$p")"
    $DRY git -C "$WS/nuttx" apply "$p"
  done
fi

cat <<'EOF'

==> 完成。编译（在 openvela 工作区根目录执行）：
    ./build.sh vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/configs/nsh \
        --cmake -c $PWD/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc -j8
    产物：cmake_out/muse_pi_pro_rcpu_nsh/nuttx（ELF32，entry 0x30000100）

==> 烧录（J-Link + OpenOCD，GDB :1024）：见 board/contest_board/k1-rcpu/docs/adaptation-guide.md §4
EOF
