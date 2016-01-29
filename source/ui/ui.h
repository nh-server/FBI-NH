#pragma once

#include <stdbool.h>

typedef struct ui_view_s {
    const char* name;
    const char* info;
    void* data;
    void (*update)(struct ui_view_s* view, void* data, float bx1, float by1, float bx2, float by2);
    void (*drawTop)(struct ui_view_s* view, void* data, float x1, float y1, float x2, float y2);
    void (*drawBottom)(struct ui_view_s* view, void* data, float x1, float y1, float x2, float y2);
} ui_view;

bool ui_push(ui_view* view);
ui_view* ui_peek();
ui_view* ui_pop();
void ui_update();
void ui_draw();