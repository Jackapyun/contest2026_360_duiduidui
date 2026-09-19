# ##############################################################################
# vendor/SpaceMiT/chips/k1-rcpu/CMakeLists.txt
#
# SpacemiT K1 RCPU (Nuclei N308) 芯片层
# 通过 CONFIG_ARCH_CHIP_CUSTOM 符号链接为 arch/risc-v/src/chip 参与构建。
#
# 参考：
#   - esOS (RT-Thread) libcpu/risc-v/spacemit/n308/
#   - Zephyr PR #95248 (soc/spacemit/k1 + arch/risc-v/custom/nuclei)
#   - NuttX rv32m1 (32 位 M-Mode 芯片层模板)
# ##############################################################################

set(SRCS k1r_head.S)

list(
  APPEND
  SRCS
  k1r_start.c
  k1r_irq.c
  k1r_irq_dispatch.c
  k1r_timerisr.c
  k1r_allocateheap.c
  k1r_userspace.c
  k1r_lowputc.c
  k1r_serial.c)

target_sources(arch PRIVATE ${SRCS})

target_include_directories(arch PRIVATE ${NUTTX_CHIP_ABS_DIR}/include)
