#pragma once

#include "ui.h"

#define PROGRESS_TEXT_MAX 512

ui_view* progressbar_create(const char* name, const char* info, void* data, void (*update)(ui_view* view, void* data, float* progress, char* progressText),
                                                                            void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2));
void progressbar_destroy(ui_view* view);
void* progressbar_get_data(ui_view* view);
char* progressbar_get_progress_text(ui_view* view);