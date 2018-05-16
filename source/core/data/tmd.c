#include <3ds.h>

#include "tmd.h"
#include "../core.h"

#define NUM_SIG_TYPES 6
static u32 sigSizes[NUM_SIG_TYPES] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};

static Result tmd_get(u8** out, u8* tmd, size_t tmdSize, u32 pos, size_t fieldSize) {
    if(tmd == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    if(tmdSize < 4) {
        return R_APP_BAD_DATA;
    }

    u8 sigType = tmd[0x03];
    if(sigType >= NUM_SIG_TYPES) {
        return R_APP_BAD_DATA;
    }

    u32 offset = sigSizes[sigType] + pos;
    if(offset + fieldSize > tmdSize) {
        return R_APP_BAD_DATA;
    }

    if(out != NULL) {
        *out = &tmd[offset];
    }

    return 0;
}

Result tmd_get_title_id(u64* titleId, u8* tmd, size_t size) {
    u8* data = NULL;
    Result res = tmd_get(&data, tmd, size, 0x4C, sizeof(u64));
    if(R_FAILED(res)) {
        return res;
    }

    if(titleId != NULL) {
        *titleId = __builtin_bswap64(*(u64*) data);
    }

    return 0;
}

Result tmd_get_content_count(u16* contentCount, u8* tmd, size_t size) {
    u8* data = NULL;
    Result res = tmd_get(&data, tmd, size, 0x9E, sizeof(u16));
    if(R_FAILED(res)) {
        return res;
    }

    if(contentCount != NULL) {
        *contentCount = __builtin_bswap16(*(u16*) data);
    }

    return 0;
}

Result tmd_get_content_id(u32* id, u8* tmd, size_t size, u32 num) {
    u8* data = NULL;
    Result res = tmd_get(&data, tmd, size, 0x9C4 + (num * 0x30), sizeof(u32));
    if(R_FAILED(res)) {
        return res;
    }

    if(id != NULL) {
        *id = __builtin_bswap32(*(u32*) data);
    }

    return 0;
}

Result tmd_get_content_index(u16* index, u8* tmd, size_t size, u32 num) {
    u8* data = NULL;
    Result res = tmd_get(&data, tmd, size, 0x9C4 + (num * 0x30) + 0x4, sizeof(u16));
    if(R_FAILED(res)) {
        return res;
    }

    if(index != NULL) {
        *index = __builtin_bswap16(*(u16*) data);
    }

    return 0;
}