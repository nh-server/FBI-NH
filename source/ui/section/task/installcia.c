#include <malloc.h>
#include <string.h>

#include <3ds.h>
#include <errno.h>

#include "../../list.h"
#include "../../error.h"
#include "task.h"

typedef struct {
    install_cia_result* result;
    u64 size;
    void* data;
    Result (*read)(void* data, u32* bytesRead, void* buffer, u32 size);

    Handle cancelEvent;
} install_cia_data;

#define bswap_64(x) \
({ \
	uint64_t __x = (x); \
	((uint64_t)( \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	    (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})

static u32 align(u32 offset, u32 alignment) {
    return (offset + (alignment - 1)) & ~(alignment - 1);
}

static void task_install_cia_thread(void* arg) {
    install_cia_data* data = (install_cia_data*) arg;

    memset(data->result, 0, sizeof(install_cia_result));

    u32 bufferSize = 1024 * 256;
    u8* buffer = (u8*) calloc(1, bufferSize);
    if(buffer != NULL) {
        bool firstBlock = true;
        u64 titleId = 0;

        Handle ciaHandle = 0;

        u32 bytesRead = 0;
        u32 bytesWritten = 0;
        u64 offset = 0;
        while(offset < data->size) {
            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                data->result->cancelled = true;
                break;
            }

            u32 readSize = bufferSize;
            if(data->size - offset < readSize) {
                readSize = (u32) (data->size - offset);
            }

            Result readRes = 0;
            if(R_FAILED(readRes = data->read(data->data, &bytesRead, buffer, readSize))) {
                if(readRes == -1) {
                    data->result->ioerr = true;
                    data->result->ioerrno = errno;
                } else {
                    data->result->result = readRes;
                }

                break;
            }

            if(firstBlock) {
                firstBlock = false;

                u32 headerSize = *(u32*) &buffer[0x00];
                u32 certSize = *(u32*) &buffer[0x08];
                titleId = bswap_64(*(u64*) &buffer[align(headerSize, 64) + align(certSize, 64) + 0x1DC]);

                FS_MediaType dest = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;

                u8 n3ds = false;
                if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
                    data->result->wrongSystem = true;
                    break;
                }

                AM_DeleteTitle(dest, titleId);
                AM_DeleteTicket(titleId);

                if(dest == 1) {
                    AM_QueryAvailableExternalTitleDatabase(NULL);
                }

                if(R_FAILED(data->result->result = AM_StartCiaInstall(dest, &ciaHandle))) {
                    break;
                }
            }

            if(R_FAILED(data->result->result = FSFILE_Write(ciaHandle, &bytesWritten, offset, buffer, bytesRead, 0))) {
                break;
            }

            offset += bytesRead;
        }

        free(buffer);

        if(ciaHandle != 0) {
            if(R_FAILED(data->result->result)) {
                AM_CancelCIAInstall(ciaHandle);
            } else if(R_SUCCEEDED(data->result->result = AM_FinishCiaInstall(ciaHandle))) {
                if(titleId == 0x0004013800000002 || titleId == 0x0004013820000002) {
                    data->result->result = AM_InstallFirm(titleId);
                }
            }
        }
    } else {
        data->result->result = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
    }

    if(R_FAILED(data->result->result) || data->result->cancelled || data->result->ioerr || data->result->wrongSystem) {
        data->result->failed = true;
    }

    data->result->finished = true;

    svcCloseHandle(data->cancelEvent);
    free(data);
}

Handle task_install_cia(install_cia_result* result, u64 size, void* data, Result (*read)(void* data, u32* bytesRead, void* buffer, u32 size)) {
    if(result == NULL || size == 0 || read == NULL) {
        return 0;
    }

    install_cia_data* installData = (install_cia_data*) calloc(1, sizeof(install_cia_data));
    installData->result = result;
    installData->size = size;
    installData->data = data;
    installData->read = read;

    Result eventRes = svcCreateEvent(&installData->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, eventRes, "Failed to create CIA installation cancel event.");

        free(installData);
        return 0;
    }

    if(threadCreate(task_install_cia_thread, installData, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, "Failed to create CIA installation thread.");

        svcCloseHandle(installData->cancelEvent);
        free(installData);
        return 0;
    }

    return installData->cancelEvent;
}