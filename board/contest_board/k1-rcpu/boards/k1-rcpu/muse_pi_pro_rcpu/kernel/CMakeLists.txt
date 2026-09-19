# ##############################################################################
# vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/kernel/CMakeLists.txt
#
# PROTECTED build 的用户 blob（nuttx_user）源：user_space 结构体。
# 参考 boards/risc-v/qemu-rv/rv-virt/kernel/CMakeLists.txt
# ##############################################################################

target_sources(nuttx_user PRIVATE k1r_userspace.c)
