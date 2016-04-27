#include <3ds.h>

#include "task.h"
#include "../../../core/util.h"

static bool task_quit;

static Handle task_pause_event;

static aptHookCookie cookie;

static void task_apt_hook(APT_HookType hook, void* param) {
    switch(hook) {
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            svcSignalEvent(task_pause_event);
            break;
        case APTHOOK_ONSUSPEND:
        case APTHOOK_ONSLEEP:
            svcClearEvent(task_pause_event);
            break;
        default:
            break;
    }
}

void task_init() {
    task_quit = false;

    Result res = 0;
    if(R_FAILED(res = svcCreateEvent(&task_pause_event, 1))) {
        util_panic("Failed to create task awake event: 0x%08lX", res);
        return;
    }

    svcSignalEvent(task_pause_event);

    aptHook(&cookie, task_apt_hook, NULL);
}

void task_exit() {
    task_quit = true;

    aptUnhook(&cookie);

    if(task_pause_event != 0) {
        svcCloseHandle(task_pause_event);
        task_pause_event = 0;
    }
}

bool task_is_quit_all() {
    return task_quit;
}

Handle task_get_pause_event() {
    return task_pause_event;
}