#include <string.h>

#include <3ds.h>

#include "patcher.h"

#pragma pack(1)
typedef struct KBlockInfo {
    u32 section_start;
    u32 page_count;
} KBlockInfo;

typedef struct KLinkedListNode {
    struct KLinkedListNode* next;
    struct KLinkedListNode* prev;
    void* data;
} KLinkedListNode;

typedef struct MemSectionInfo {
    u8 padding[0x0C - 0x00];
    KLinkedListNode* first_node;
    KLinkedListNode* last_node;
} MemSectionInfo;

typedef struct KCodeSet {
    u8 padding0[0x08 - 0x00];
    MemSectionInfo text_info;
    u8 padding1[0x64 - 0x1C];
} KCodeSet;
#pragma pack(0)

u32 kprocess_ptr = 0;
u32 kprocess_size = 0;
u32 kprocess_code_set_offset = 0;
u32 kprocess_pid_offset = 0;

s32 kernel_patch_fs() {
    asm volatile("cpsid aif");

    u32 currProcessPtr = *(u32*) kprocess_ptr;
    u32 vtablePtr = *(u32*) currProcessPtr;

    for(u32 processPtr = currProcessPtr; *(u32*) processPtr == vtablePtr; processPtr -= kprocess_size) {
        if(*(u32*) (processPtr + kprocess_pid_offset) == 0) {
            KCodeSet* codeSet = *(KCodeSet**) (processPtr + kprocess_code_set_offset);
            if(codeSet != NULL) {
                // Patches out an archive access check.
                u8 original[] = {0x0C, 0x05, 0x0C, 0x33, 0x46, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x28, 0x01, 0xD0, 0x00, 0x20, 0xF8};
                u8 patched[] =  {0x0C, 0x05, 0x0C, 0x33, 0x46, 0x01, 0x20, 0x00, 0x00, 0x00, 0x28, 0x01, 0xD0, 0x00, 0x20, 0xF8};

                for(KLinkedListNode* node = codeSet->text_info.first_node; node != NULL; node = node->next) {
                    KBlockInfo* blockInfo = (KBlockInfo*) node->data;
                    u32 blockSize = blockInfo->page_count * 0x1000;

                    bool done = false;
                    for(u32 i = 0; i <= blockSize - sizeof(original); i++) {
                        u8* dst = (u8*) (blockInfo->section_start + i);

                        bool equal = true;
                        for(u32 b = 0; b < sizeof(original); b++) {
                            if(original[b] != 0xFF && dst[b] != original[b]) {
                                equal = false;
                                break;
                            }
                        }

                        if(equal) {
                            for(u32 b = 0; b < sizeof(patched); b++) {
                                dst[b] = patched[b];
                            }

                            done = true;
                            break;
                        }
                    }

                    if(done || node == codeSet->text_info.last_node) {
                        break;
                    }
                }
            }

            break;
        }
    }

    return 0;
}

u32 old_pid = 0;

s32 kernel_patch_pid_zero() {
    u32* pidPtr = (u32*) (*(u32*) kprocess_ptr + kprocess_pid_offset);

    old_pid = *pidPtr;
    *pidPtr = 0;

    return 0;
}

s32 kernel_patch_pid_reset() {
    u32* pidPtr = (u32*) (*(u32*) kprocess_ptr + kprocess_pid_offset);

    *pidPtr = old_pid;

    return 0;
}

void apply_patches() {
    kprocess_ptr = 0xFFFF9004;

    if(osGetKernelVersion() < 0x022C0600) {
        kprocess_size = 0x260;
        kprocess_code_set_offset = 0xA8;
        kprocess_pid_offset = 0xAC;
    } else {
        bool n3ds = false;
        APT_CheckNew3DS((u8*) &n3ds);

        if(n3ds) {
            kprocess_size = 0x270;
            kprocess_code_set_offset = 0xB8;
            kprocess_pid_offset = 0xBC;
        } else {
            kprocess_size = 0x268;
            kprocess_code_set_offset = 0xB0;
            kprocess_pid_offset = 0xB4;
        }
    }

    if(osGetKernelVersion() > 0x022E0000) {
        svcBackdoor(kernel_patch_pid_zero);
        srvExit();
        srvInit();
        svcBackdoor(kernel_patch_pid_reset);
    }

    svcBackdoor(kernel_patch_fs);
}