#include <malloc.h>

#include <3ds.h>

#include "core/screen.h"
#include "core/util.h"
#include "svchax/svchax.h"
#include "ui/mainmenu.h"
#include "ui/ui.h"
#include "ui/section/action/clipboard.h"
#include "ui/section/task/task.h"

static void* soc_buffer;

void cleanup() {
    clipboard_clear();

    task_exit();
    ui_exit();
    screen_exit();

    socExit();
    if(soc_buffer != NULL) {
        free(soc_buffer);
        soc_buffer = NULL;
    }

    amExit();
    httpcExit();
    ptmuExit();
    acExit();
    cfguExit();
    romfsExit();
    gfxExit();
}

int main(int argc, const char* argv[]) {
    gfxInitDefault();

    if(argc > 0) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            util_panic("Failed to acquire kernel access.");
            return 1;
        }
    }

    aptOpenSession();
    Result setCpuTimeRes = APT_SetAppCpuTimeLimit(30);
    aptCloseSession();

    if(R_FAILED(setCpuTimeRes)) {
        util_panic("Failed to set syscore CPU time limit: %08lX", setCpuTimeRes);
        return 1;
    }

    romfsInit();
    cfguInit();
    acInit();
    ptmuInit();
    httpcInit(0);

    amInit();
    AM_InitializeExternalTitleDatabase(false);

    soc_buffer = memalign(0x1000, 0x100000);
    if(soc_buffer != NULL) {
        socInit(soc_buffer, 0x100000);
    }

    screen_init();
    ui_init();
    task_init();

    mainmenu_open();

    while(aptMainLoop() && ui_update());

    cleanup();
    return 0;
}
