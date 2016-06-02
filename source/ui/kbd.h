#pragma once

typedef struct ui_view_s ui_view;

void kbd_display(const char* name, const char* initialInput, void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2),
                                                                         void (*finished)(void* data, char* input),
                                                                         void (*canceled)(void* data));