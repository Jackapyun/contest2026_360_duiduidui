/****************************************************************************
 * vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/scripts/knsh_user.ld
 *
 * K1 RCPU (N308) PROTECTED build 用户 blob 链接脚本
 * 用户 blob 从 CONFIG_NUTTX_USERSPACE 起，包含 user_space + 用户代码/数据/堆。
 * 参考 qemu-rv scripts/ld-userland.script + c906 user-space.ld。
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef CONFIG_NUTTX_USERSPACE_SIZE
#  define CONFIG_NUTTX_USERSPACE_SIZE       0x00100000
#endif

OUTPUT_ARCH("riscv")

SECTIONS
{
    /* 用户 blob 基址 = CONFIG_NUTTX_USERSPACE (0x30080000) */
    . = CONFIG_NUTTX_USERSPACE;

    /* .userspace：包含 struct userspace_s user_space（必须位于 CONFIG_NUTTX_USERSPACE） */
    .userspace : {
        KEEP(*(.userspace))
    }

    .text : {
        _stext = ABSOLUTE(.);
        *(.text .text.*)
        *(.fixup)
        *(.gnu.warning)
        *(.rodata .rodata.* .srodata .srodata.*)
        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)
        *(.got)
        *(.gcc_except_table)
        *(.gnu.linkonce.r.*)
        _etext = ABSOLUTE(.);
    }

    .init_section : {
        _sinit = ABSOLUTE(.);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        _einit = ABSOLUTE(.);
    }

    __exidx_start = ABSOLUTE(.);
    __exidx_end = ABSOLUTE(.);

    _eronly = ABSOLUTE(.);

    .data : {
        _sdata = ABSOLUTE(.);
        *(.data .data.*)
        *(.sdata .sdata.* .sdata2.*)
        *(.gnu.linkonce.d.*)
        CONSTRUCTORS
        . = ALIGN(4);
        _edata = ABSOLUTE(.);
    }

    .bss : {
        _sbss = ABSOLUTE(.);
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = ABSOLUTE(.);
    }

    /* 用户 RAM 终点 = 用户堆顶（us_heapend）。用户堆=us_bssend..__ld_usram_end */
    __ld_usram_end = CONFIG_NUTTX_USERSPACE + CONFIG_NUTTX_USERSPACE_SIZE;

    /* Debug sections */
    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .comment 0 : { *(.comment) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_info 0 : { *(.debug_info) }
    .debug_line 0 : { *(.debug_line) }
    .debug_aranges 0 : { *(.debug_aranges) }

    /* 丢弃 build-id note，确保 .userspace 落在 CONFIG_NUTTX_USERSPACE（否则
     * .note.gnu.build-id 会占 0x30080000，把 user_space 推到 +0x24，导致
     * USERSPACE 宏与 struct 错位、k1r_userspace 读到垃圾 us_bssstart）。 */
    /DISCARD/ : { *(.note.gnu.build-id) *(.note) }
}
