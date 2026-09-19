# ##############################################################################
# vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/CMakeLists.txt
#
# SpacemiT MUSE Pi Pro RCPU（N308）板级
# ##############################################################################

add_subdirectory(src)

file(GLOB EXTRA_LIBS
     ${NUTTX_BOARD_ABS_DIR}/libs/${CONFIG_ARCH_BOARD_CUSTOM_NAME}/*.a)

set_property(GLOBAL APPEND PROPERTY NUTTX_EXTRA_LIBRARIES ${EXTRA_LIBS})

set_property(GLOBAL PROPERTY LD_SCRIPT ${NUTTX_BOARD_ABS_DIR}/scripts/ld.script)

if(CONFIG_BUILD_PROTECTED)
  # PROTECTED：用户 blob 用 knsh_user.ld 链到 CONFIG_NUTTX_USERSPACE。
  # 内核 LD_SCRIPT 保持 scripts/ld.script（用 CONFIG_RAM_START=0x30000000）。
  add_subdirectory(kernel)
  set_property(GLOBAL PROPERTY LD_SCRIPT_USER
               ${CMAKE_CURRENT_LIST_DIR}/scripts/knsh_user.ld)
endif()
