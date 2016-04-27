#pragma once

typedef struct ui_view_s ui_view;

void prompt_display(const char* name, const char* text, u32 rgba, bool option, void* data, void (*update)(ui_view* view, void* data),
                                                                                           void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2),
                                                                                           void (*onResponse)(ui_view* view, void* data, bool response));