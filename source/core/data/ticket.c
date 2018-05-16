#include <3ds.h>

#include "ticket.h"
#include "../core.h"

#define NUM_SIG_TYPES 6
static u32 sigSizes[NUM_SIG_TYPES] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};

Result ticket_get_title_id(u64* titleId, u8* ticket, size_t size) {
    if(ticket == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    if(size < 4) {
        return R_APP_BAD_DATA;
    }

    u8 sigType = ticket[0x03];
    if(sigType >= NUM_SIG_TYPES) {
        return R_APP_BAD_DATA;
    }

    u32 offset = sigSizes[sigType] + 0x9C;
    if(offset + sizeof(u64) > size) {
        return R_APP_BAD_DATA;
    }

    if(titleId != NULL) {
        *titleId = __builtin_bswap64(*(u64*) &ticket[offset]);
    }

    return 0;
}