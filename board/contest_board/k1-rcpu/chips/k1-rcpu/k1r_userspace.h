/****************************************************************************
 * vendor/SpaceMiT/chips/k1-rcpu/k1r_userspace.h
 *
 * SpacemiT K1 RCPU (Nuclei N308) PROTECTED build 用户态初始化
 ****************************************************************************/

#ifndef __VENDOR_SPACEMIT_CHIPS_K1_RCPU_K1R_USERSPACE_H
#define __VENDOR_SPACEMIT_CHIPS_K1_RCPU_K1R_USERSPACE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: k1r_userspace
 *
 * Description:
 *   清用户 .bss、拷贝用户 .data、配置 PMP 内核/用户隔离。
 *   仅 PROTECTED build；在内核启动、跳转用户态前调用。
 *
 ****************************************************************************/

#ifdef CONFIG_BUILD_PROTECTED
void k1r_userspace(void);
#endif

#endif /* __VENDOR_SPACEMIT_CHIPS_K1_RCPU_K1R_USERSPACE_H */
