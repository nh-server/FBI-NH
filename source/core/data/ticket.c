#include <3ds.h>

#include "ticket.h"

static u32 sigSizes[6] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};

u64 ticket_get_title_id(u8* ticket) {
    return __builtin_bswap64(*(u64*) &ticket[sigSizes[ticket[0x03]] + 0x9C]);
}