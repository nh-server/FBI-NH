#ifndef __SVCHAX_H__
#define _SVCHAX_H__

/*
 * for 3DSX builds, svchax_init expects that:
 *
 * - gfxInit was already called.
 * - new 3DS higher clockrate and L2 cache are disabled.
 * - there is at least 64 KBytes (16 pages) of unallocated linear memory.
 *   ( the current 3dsx loaders and ctrulib's default allocator will keep 1MB
 *     of unallocated linear memory, so this is only relevant when using
 *     a custom allocator)
 *
 *
 * svchax_init will grant full svc access to the calling thread and process
 * up to system version 10.7 (kernel version 2.50-11), by using:
 * - memchunkhax1 for kernel version <= 2.46-0
 * - memchunkhax2 for 2.46-0 < kernel version <= 2.50-11
 *
 * access to privileged services can also be obtained by calling
 * svchax_init with patch_srv set to true.
 *
 * __ctr_svchax and __ctr_svchax_srv will reflect the current
 * status of the privileged access for svc calls and services respectively.
 *
 * svchax assumes that CIA builds already have access to svcBackdoor
 * and will skip running memchunkhax there.
 *
 */

#include <3ds/types.h>

#ifdef __cplusplus
extern "C" {
#endif

Result svchax_init(bool patch_srv);

extern u32 __ctr_svchax;
extern u32 __ctr_svchax_srv;

#ifdef __cplusplus
}
#endif


#endif //_SVCHAX_H__
