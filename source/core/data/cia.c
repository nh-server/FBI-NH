#include <3ds.h>

#include "cia.h"
#include "smdh.h"
#include "tmd.h"
#include "../error.h"

Result cia_get_title_id(u64* titleId, u8* cia, size_t size) {
    if(cia == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    if(size < 0x10) {
        return R_APP_BAD_DATA;
    }

    u32 headerSize = ((*(u32*) &cia[0x00]) + 0x3F) & ~0x3F;
    u32 certSize = ((*(u32*) &cia[0x08]) + 0x3F) & ~0x3F;
    u32 ticketSize = ((*(u32*) &cia[0x0C]) + 0x3F) & ~0x3F;
    u32 offset = headerSize + certSize + ticketSize;

    if(offset >= size) {
        return R_APP_BAD_DATA;
    }

    return tmd_get_title_id(titleId, &cia[offset], size - offset);
}

Result cia_file_get_smdh(SMDH* smdh, Handle handle) {
    Result res = 0;

    if(smdh != NULL) {
        u32 bytesRead = 0;

        u32 header[8];
        if(R_SUCCEEDED(res = FSFILE_Read(handle, &bytesRead, 0, header, sizeof(header))) && bytesRead == sizeof(header)) {
            u32 headerSize = (header[0] + 0x3F) & ~0x3F;
            u32 certSize = (header[2] + 0x3F) & ~0x3F;
            u32 ticketSize = (header[3] + 0x3F) & ~0x3F;
            u32 tmdSize = (header[4] + 0x3F) & ~0x3F;
            u32 metaSize = (header[5] + 0x3F) & ~0x3F;
            u64 contentSize = ((header[6] | ((u64) header[7] << 32)) + 0x3F) & ~0x3F;

            if(metaSize >= 0x3AC0) {
                res = FSFILE_Read(handle, &bytesRead, headerSize + certSize + ticketSize + tmdSize + contentSize + 0x400, smdh, sizeof(SMDH));
            } else {
                res = R_APP_BAD_DATA;
            }
        }
    } else {
        res = R_APP_INVALID_ARGUMENT;
    }

    return res;
}