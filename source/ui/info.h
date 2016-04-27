#pragma once

#define PROGRESS_TEXT_MAX 512

typedef struct ui_view_s ui_view;

void info_display(const char* name, const char* info, bool bar, void* data, void (*update)(ui_view* view, void* data, float* progress, char* text),
                                                                            void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2));
void info_destroy(ui_view* view);