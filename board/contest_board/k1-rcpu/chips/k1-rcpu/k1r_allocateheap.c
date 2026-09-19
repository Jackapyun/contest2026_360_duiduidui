/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_allocateheap.c
 *
 * SpacemiT K1 RCPU (Nuclei N308) 堆分配
 *
 * PROTECTED 下拆分内核/用户堆（参考 c906_allocateheap.c）：
 *   内核堆 : g_idle_topstack -> CONFIG_NUTTX_USERSPACE (M-only)
 *   用户堆 : USERSPACE->us_bssend -> us_heapend (用户区, U)
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/userspace.h>
#include <arch/board/board.h>

#include "riscv_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_allocate_heap
 *
 * Description:
 *   PROTECTED + MM_KERNEL_HEAP：返回用户堆（us_bssend -> us_heapend）。
 *   FLAT：返回整个堆（g_idle_topstack -> RAM_END）。
 ****************************************************************************/

void up_allocate_heap(void **heap_start, size_t *heap_size)
{
#if defined(CONFIG_BUILD_PROTECTED) && defined(CONFIG_MM_KERNEL_HEAP)
  uintptr_t ubase = (uintptr_t)USERSPACE->us_bssend;
  uintptr_t utop  = (uintptr_t)USERSPACE->us_heapend;
  size_t    usize = utop - ubase;

  board_autoled_on(LED_HEAPALLOCATE);

  *heap_start = (void *)ubase;
  *heap_size  = usize;

  /* PMP 已在 k1r_userspace() 里给用户区 U 权限 */
#else
  board_autoled_on(LED_HEAPALLOCATE);
  *heap_start = (void *)g_idle_topstack;
  *heap_size  = CONFIG_RAM_END - g_idle_topstack;
#endif /* CONFIG_BUILD_PROTECTED && CONFIG_MM_KERNEL_HEAP */
}

/****************************************************************************
 * Name: up_allocate_kheap
 *
 * Description:
 *   PROTECTED + MM_KERNEL_HEAP：返回内核堆（g_idle_topstack -> 用户区基址，M-only）。
 ****************************************************************************/

#if defined(CONFIG_BUILD_PROTECTED) && defined(CONFIG_MM_KERNEL_HEAP) && \
    defined(__KERNEL__)
void up_allocate_kheap(void **heap_start, size_t *heap_size)
{
  *heap_start = (void *)g_idle_topstack;
  *heap_size  = CONFIG_NUTTX_USERSPACE - g_idle_topstack;
}
#endif
