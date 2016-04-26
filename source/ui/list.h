#pragma once

#include <sys/syslimits.h>

#include "ui.h"
#include "../core/linkedlist.h"

typedef struct {
    char name[NAME_MAX];
    u32 color;
    void* data;
} list_item;

void list_display(const char* name, const char* info, void* data, void (*update)(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched),
                                                                  void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected));
void list_destroy(ui_view* view);