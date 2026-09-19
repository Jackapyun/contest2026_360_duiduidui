/****************************************************************************
 * vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/kernel/k1r_userspace.c
 *
 * SpacemiT MUSE Pi Pro RCPU (N308) PROTECTED build 用户空间定义
 * （用户 blob 里定义 struct userspace_s user_space，进 .userspace 段）
 * 参考 boards/risc-v/c906/.../kernel/c906_userspace.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdlib.h>

#include <nuttx/arch.h>
#include <nuttx/mm/mm.h>
#include <nuttx/wqueue.h>
#include <nuttx/userspace.h>

#if defined(CONFIG_BUILD_PROTECTED) && !defined(__KERNEL__)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_NUTTX_USERSPACE
#  error "CONFIG_NUTTX_USERSPACE not defined"
#endif

#if CONFIG_NUTTX_USERSPACE != 0x30080000
#  error "CONFIG_NUTTX_USERSPACE must match knsh_user.ld (. = 0x30080000)"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct userspace_data_s g_userspace_data =
{
  .us_heap = &g_mmheap,
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* 这些地址由 knsh_user.ld 链接脚本设置 */

extern uint8_t _stext[];           /* Start of .text */
extern uint8_t _etext[];           /* End of .text + .rodata */
extern const uint8_t _eronly[];    /* End+1 of read only (.text+.rodata) */
extern uint8_t _sdata[];           /* Start of .data */
extern uint8_t _edata[];           /* End+1 of .data */
extern uint8_t _sbss[];            /* Start of .bss */
extern uint8_t _ebss[];            /* End+1 of .bss */

extern uint8_t __ld_usram_end[];   /* End+1 of user ram section */

const struct userspace_s userspace locate_data(".userspace") =
{
  /* General memory map */

  .us_entrypoint    = nxuser_init,
  .us_textstart     = (uintptr_t)_stext,
  .us_textend       = (uintptr_t)_etext,
  .us_datasource    = (uintptr_t)_eronly,
  .us_datastart     = (uintptr_t)_sdata,
  .us_dataend       = (uintptr_t)_edata,
  .us_bssstart      = (uintptr_t)_sbss,
  .us_bssend        = (uintptr_t)_ebss,

  .us_heapend       = (uintptr_t)__ld_usram_end,

  /* User data memory structure */

  .us_data          = &g_userspace_data,

  /* Task/thread startup routines */

  .task_startup     = nxtask_startup,

  /* Signal handler trampoline */

#ifndef CONFIG_DISABLE_SIGNALS
  .signal_handler   = up_signal_handler,
#endif

  /* User-space work queue support */

#ifdef CONFIG_LIBC_USRWORK
  .work_usrstart    = work_usrstart,
#endif

  /* Builtin support */

#ifdef CONFIG_BUILTIN
  .builtin_count    = &g_builtin_count,
  .builtins         = &g_builtins[0],
#endif
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#endif /* CONFIG_BUILD_PROTECTED && !__KERNEL__ */
