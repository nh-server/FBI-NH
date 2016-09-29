#include <malloc.h>

#include <3ds.h>

#include "core/clipboard.h"
#include "core/screen.h"
#include "core/util.h"
#include "svchax/svchax.h"
#include "ui/error.h"
#include "ui/mainmenu.h"
#include "ui/ui.h"
#include "ui/section/task/task.h"

static void* soc_buffer = NULL;
static u32 old_time_limit = UINT32_MAX;

void cleanup_services() {
    socExit();
    if(soc_buffer != NULL) {
        free(soc_buffer);
        soc_buffer = NULL;
    }

    httpcExit();
    pxiDevExit();
    ptmuExit();
    acExit();
    cfguExit();
    amExit();
}

Result init_services() {
    Result res = 0;

    Handle tempAM = 0;
    if(R_SUCCEEDED(res = srvGetServiceHandle(&tempAM, "am:net"))) {
        svcCloseHandle(tempAM);

        if(R_SUCCEEDED(res = amInit())
           && R_SUCCEEDED(res = cfguInit())
           && R_SUCCEEDED(res = acInit())
           && R_SUCCEEDED(res = ptmuInit())
           && R_SUCCEEDED(res = pxiDevInit())
           && R_SUCCEEDED(res = httpcInit(0))) {
            soc_buffer = memalign(0x1000, 0x100000);
            if(soc_buffer != NULL) {
                res = socInit(soc_buffer, 0x100000);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }
    }

    if(R_FAILED(res)) {
        cleanup_services();
    }

    return res;
}

void cleanup() {
    clipboard_clear();

    task_exit();
    ui_exit();
    screen_exit();

    if(old_time_limit != UINT32_MAX) {
        APT_SetAppCpuTimeLimit(old_time_limit);
    }

    cleanup_services();

    romfsExit();

    gfxExit();
}

void init() {
    gfxInitDefault();

    Result romfsRes = romfsInit();
    if(R_FAILED(romfsRes)) {
        util_panic("Failed to mount RomFS: %08lX", romfsRes);
        return;
    }

    if(R_FAILED(init_services())) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            util_panic("Failed to acquire kernel access.");
            return;
        }

        Result initRes = init_services();
        if(R_FAILED(initRes)) {
            util_panic("Failed to initialize services: %08lX", initRes);
            return;
        }
    }

    APT_GetAppCpuTimeLimit(&old_time_limit);
    Result cpuRes = APT_SetAppCpuTimeLimit(30);
    if(R_FAILED(cpuRes)) {
        util_panic("Failed to set syscore CPU time limit: %08lX", cpuRes);
        return;
    }

    AM_InitializeExternalTitleDatabase(false);

    screen_init();
    ui_init();
    task_init();
}

int main(int argc, const char* argv[]) {
    if(argc > 0) {
        util_set_3dsx_path(argv[0]);
    }

    init();

    mainmenu_open();
    while(aptMainLoop() && ui_update());

    cleanup();

    return 0;
}
