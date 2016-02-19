#include <3ds.h>

#include "task.h"

static bool task_quit;

bool task_is_quit_all() {
    return task_quit;
}

void task_quit_all() {
    task_quit = true;
}