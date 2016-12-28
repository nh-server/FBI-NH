#include <sys/iosupport.h>
#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "core/clipboard.h"
#include "core/screen.h"
#include "core/util.h"
#include "hax/khax.h"
#include "ui/error.h"
#include "ui/mainmenu.h"
#include "ui/ui.h"
#include "ui/section/task/task.h"

static bool am_initialized = false;
static bool cfgu_initialized = false;
static bool ac_initialized = false;
static bool ptmu_initialized = false;
static bool pxidev_initialized = false;
static bool httpc_initialized = false;
static bool soc_initialized = false;

static void* soc_buffer = NULL;
static u32 old_time_limit = UINT32_MAX;

void cleanup_services() {
    if(soc_initialized) {
        socExit();
        if(soc_buffer != NULL) {
            free(soc_buffer);
            soc_buffer = NULL;
        }

        soc_initialized = false;
    }

    if(httpc_initialized) {
        httpcExit();
        httpc_initialized = false;
    }

    if(pxidev_initialized) {
        pxiDevExit();
        pxidev_initialized = false;
    }

    if(ptmu_initialized) {
        ptmuExit();
        ptmu_initialized = false;
    }

    if(ac_initialized) {
        acExit();
        ac_initialized = false;
    }

    if(cfgu_initialized) {
        cfguExit();
        cfgu_initialized = false;
    }

    if(am_initialized) {
        amExit();
        am_initialized = false;
    }
}

Result init_services() {
    Result res = 0;

    Handle tempAM = 0;
    if(R_SUCCEEDED(res = srvGetServiceHandle(&tempAM, "am:net"))) {
        svcCloseHandle(tempAM);

        if(R_SUCCEEDED(res = amInit()) && (am_initialized = true)
           && R_SUCCEEDED(res = cfguInit()) && (cfgu_initialized = true)
           && R_SUCCEEDED(res = acInit()) && (ac_initialized = true)
           && R_SUCCEEDED(res = ptmuInit()) && (ptmu_initialized = true)
           && R_SUCCEEDED(res = pxiDevInit()) && (pxidev_initialized = true)
           && R_SUCCEEDED(res = httpcInit(0)) && (httpc_initialized = true)) {
            soc_buffer = memalign(0x1000, 0x100000);
            if(soc_buffer != NULL) {
                if(R_SUCCEEDED(res = socInit(soc_buffer, 0x100000))) {
                    soc_initialized = true;
                }
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
        const devoptab_t* oldStdOut = devoptab_list[STD_OUT];
        const devoptab_t* oldStdErr = devoptab_list[STD_ERR];

        consoleInit(GFX_TOP, NULL);
        util_store_console_std();

        if(!khax_execute()) {
            printf("Press any key to exit.\n");

            util_panic_quiet();
            return;
        }

        devoptab_list[STD_OUT] = oldStdOut;
        devoptab_list[STD_ERR] = oldStdErr;

        gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
        gfxSetDoubleBuffering(GFX_TOP, true);

        Result initRes = init_services();
        if(R_FAILED(initRes)) {
            util_panic("Failed to initialize services: %08lX", initRes);
            return;
        }
    }

    osSetSpeedupEnable(true);

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
