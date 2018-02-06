#include <sys/iosupport.h>
#include <malloc.h>

#include <3ds.h>
#include <curl/curl.h>

#include "core/clipboard.h"
#include "core/screen.h"
#include "core/util.h"
#include "core/task/task.h"
#include "ui/error.h"
#include "ui/mainmenu.h"
#include "ui/resources.h"
#include "ui/ui.h"

#define CURRENT_KPROCESS (*(void**) 0xFFFF9004)

#define KPROCESS_PID_OFFSET_OLD (0xB4)
#define KPROCESS_PID_OFFSET_NEW (0xBC)

static bool backdoor_ran = false;
static bool n3ds = false;
static u32 old_pid = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
static __attribute__((naked)) Result svcGlobalBackdoor(s32 (*callback)()) {
    asm volatile(
            "svc 0x30\n"
            "bx lr"
    );
}
#pragma GCC diagnostic pop

static s32 patch_pid_kernel() {
    u32 *pidPtr = (u32*) (CURRENT_KPROCESS + (n3ds ? KPROCESS_PID_OFFSET_NEW : KPROCESS_PID_OFFSET_OLD));

    old_pid = *pidPtr;
    *pidPtr = 0;

    backdoor_ran = true;
    return 0;
}

static s32 restore_pid_kernel() {
    u32 *pidPtr = (u32*) (CURRENT_KPROCESS + (n3ds ? KPROCESS_PID_OFFSET_NEW : KPROCESS_PID_OFFSET_OLD));

    *pidPtr = old_pid;

    backdoor_ran = true;
    return 0;
}

static bool attempt_patch_pid() {
    backdoor_ran = false;
    APT_CheckNew3DS(&n3ds);

    svcGlobalBackdoor(patch_pid_kernel);
    srvExit();
    srvInit();
    svcGlobalBackdoor(restore_pid_kernel);

    return backdoor_ran;
}

static void (*exit_funcs[16])()= {NULL};
static u32 exit_func_count = 0;

static void* soc_buffer = NULL;

void cleanup_services() {
    for(u32 i = 0; i < exit_func_count; i++) {
        if(exit_funcs[i] != NULL) {
            exit_funcs[i]();
            exit_funcs[i] = NULL;
        }
    }

    exit_func_count = 0;

    if(soc_buffer != NULL) {
        free(soc_buffer);
        soc_buffer = NULL;
    }
}

#define INIT_SERVICE(initStatement, exitFunc) (R_SUCCEEDED(res = (initStatement)) && (exit_funcs[exit_func_count++] = (exitFunc)))

Result init_services() {
    Result res = 0;

    soc_buffer = memalign(0x1000, 0x100000);
    if(soc_buffer != NULL) {
        Handle tempAM = 0;
        if(R_SUCCEEDED(res = srvGetServiceHandle(&tempAM, "am:net"))) {
            svcCloseHandle(tempAM);

            if(INIT_SERVICE(amInit(), amExit)
               && INIT_SERVICE(cfguInit(), cfguExit)
               && INIT_SERVICE(acInit(), acExit)
               && INIT_SERVICE(ptmuInit(), ptmuExit)
               && INIT_SERVICE(pxiDevInit(), pxiDevExit)
               && INIT_SERVICE(httpcInit(0), httpcExit)
               && INIT_SERVICE(socInit(soc_buffer, 0x100000), (void (*)()) socExit));
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    if(R_FAILED(res)) {
        cleanup_services();
    }

    return res;
}

//static u32 old_time_limit = UINT32_MAX;

FILE* dbg;

void init() {
    gfxInitDefault();

    Result romfsRes = romfsInit();
    if(R_FAILED(romfsRes)) {
        util_panic("Failed to mount RomFS: %08lX", romfsRes);
        return;
    }

    if(R_FAILED(init_services())) {
        if(!attempt_patch_pid()) {
            util_panic("Kernel backdoor not installed.\nPlease run a kernel exploit and try again.");
            return;
        }

        Result initRes = init_services();
        if(R_FAILED(initRes)) {
            util_panic("Failed to initialize services: %08lX", initRes);
            return;
        }
    }

    /*APT_GetAppCpuTimeLimit(&old_time_limit);
    Result cpuRes = APT_SetAppCpuTimeLimit(30);
    if(R_FAILED(cpuRes)) {
        util_panic("Failed to set syscore CPU time limit: %08lX", cpuRes);
        return;
    }*/

    AM_InitializeExternalTitleDatabase(false);

    dbg = fopen("sdmc:/debug.txt", "wb");

    curl_global_init(CURL_GLOBAL_ALL);

    screen_init();
    resources_load();
    ui_init();
    task_init();
}

void cleanup() {
    fclose(dbg);

    clipboard_clear();

    task_exit();
    ui_exit();
    screen_exit();

    /*if(old_time_limit != UINT32_MAX) {
        APT_SetAppCpuTimeLimit(old_time_limit);
    }*/

    cleanup_services();

    romfsExit();

    gfxExit();
}

static void main_thread(void* arg) {
    init();

    mainmenu_open();
    while(aptMainLoop() && ui_update());

    cleanup();
}

int main(int argc, const char* argv[]) {
    if(argc > 0 && envIsHomebrew()) {
        util_set_3dsx_path(argv[0]);
    }

    osSetSpeedupEnable(true);

    u32 oldTimeLimit = UINT32_MAX;
    APT_GetAppCpuTimeLimit(&oldTimeLimit);

    Result cpuRes = APT_SetAppCpuTimeLimit(30);
    if(R_FAILED(cpuRes)) {
        util_panic("Failed to set syscore CPU time limit: %08lX", cpuRes);
        return 0;
    }

    Thread mainThread = threadCreate(main_thread, NULL, 0x10000, 0x18, 1, true);
    if(mainThread == NULL) {
        util_panic("Failed to start main thread.");
        return 0;
    }

    threadJoin(mainThread, U64_MAX);

    if(oldTimeLimit != UINT32_MAX) {
        APT_SetAppCpuTimeLimit(oldTimeLimit);
    }

    osSetSpeedupEnable(false);

    return 0;
}