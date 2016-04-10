#include <malloc.h>
#include <stdlib.h>

#include <3ds.h>

#include "screen.h"
#include "util.h"
#include "libkhax/khax.h"
#include "patcher/patcher.h"
#include "ui/mainmenu.h"
#include "ui/section/action/clipboard.h"
#include "ui/section/task/task.h"

static void* soc_buffer;

void cleanup() {
    clipboard_clear();

    task_quit_all();
    screen_exit();

    socExit();
    if(soc_buffer != NULL) {
        free(soc_buffer);
        soc_buffer = NULL;
    }

    ptmuExit();
    acExit();
    amExit();
    cfguExit();
    romfsExit();
    khaxExit();
    gfxExit();
}

int main(int argc, const char* argv[]) {
    gfxInitDefault();

    if(argc > 0) {
        Result res = khaxInit();
        if(R_FAILED(res)) {
            util_panic("Failed to acquire service access: %08lX", res);
            return 1;
        }
    }

    patch_fs();

    aptOpenSession();
    Result setCpuTimeRes = APT_SetAppCpuTimeLimit(30);
    aptCloseSession();

    if(R_FAILED(setCpuTimeRes)) {
        util_panic("Failed to set syscore CPU time: %08lX", setCpuTimeRes);
        return 1;
    }

    romfsInit();
    cfguInit();
    acInit();
    ptmuInit();

    amInit();
    AM_InitializeExternalTitleDatabase(false);

    soc_buffer = memalign(0x1000, 0x100000);
    if(soc_buffer != NULL) {
        socInit(soc_buffer, 0x100000);
    }

    screen_init();

    mainmenu_open();

    while(aptMainLoop()) {
        ui_update();
        if(ui_peek() == NULL) {
            break;
        }

        ui_draw();
    }

    cleanup();
    return 0;
}
