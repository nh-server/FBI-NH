#include <3ds.h>

#include "tmd.h"

static u32 sigSizes[6] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};

u64 tmd_get_title_id(u8* tmd) {
    return __builtin_bswap64(*(u64*) &tmd[sigSizes[tmd[0x03]] + 0x4C]);
}

u16 tmd_get_content_count(u8* tmd) {
    return __builtin_bswap16(*(u16*) &tmd[sigSizes[tmd[0x03]] + 0x9E]);
}

u8* tmd_get_content_chunk(u8* tmd, u32 index) {
    return &tmd[sigSizes[tmd[0x03]] + 0x9C4 + (index * 0x30)];
}