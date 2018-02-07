#pragma once

void task_init();
void task_exit();
bool task_is_quit_all();
Handle task_get_pause_event();
Handle task_get_suspend_event();

#include "capturecam.h"
#include "dataop.h"