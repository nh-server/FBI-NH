#pragma once

typedef struct ui_view_s ui_view;

ui_view* prompt_display(const char* name, const char* text, u32 color, bool option, void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2),
                                                                                                void (*onResponse)(ui_view* view, void* data, bool response));