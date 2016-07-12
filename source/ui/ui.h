#pragma once

#include <stdbool.h>

typedef struct ui_view_s {
    const char* name;
    const char* info;
    void* data;
    void (*update)(struct ui_view_s* view, void* data, float bx1, float by1, float bx2, float by2);
    void (*drawTop)(struct ui_view_s* view, void* data, float x1, float y1, float x2, float y2);
    void (*drawBottom)(struct ui_view_s* view, void* data, float x1, float y1, float x2, float y2);

    Handle active;
} ui_view;

void ui_init();
void ui_exit();

ui_view* ui_create();
void ui_destroy(ui_view* view);

ui_view* ui_top();
bool ui_push(ui_view* view);
void ui_pop();
bool ui_update();

void ui_draw_meta_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_ext_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_file_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_pending_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_system_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_ticket_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_titledb_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);