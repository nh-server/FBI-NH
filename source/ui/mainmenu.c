#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "list.h"
#include "mainmenu.h"
#include "section/section.h"
#include "../screen.h"

#define MAINMENU_ITEM_COUNT 12

static u32 mainmenu_item_count = MAINMENU_ITEM_COUNT;
static list_item mainmenu_items[MAINMENU_ITEM_COUNT] = {
        {"SD", COLOR_TEXT, files_open_sd},
        {"CTR NAND", COLOR_TEXT, files_open_ctr_nand},
        {"TWL NAND", COLOR_TEXT, files_open_twl_nand},
        {"TWL Photo", COLOR_TEXT, files_open_twl_photo},
        {"TWL Sound", COLOR_TEXT, files_open_twl_sound},
        {"Dump NAND", COLOR_TEXT, dump_nand},
        {"Titles", COLOR_TEXT, titles_open},
        {"Pending Titles", COLOR_TEXT, pendingtitles_open},
        {"Tickets", COLOR_TEXT, tickets_open},
        {"Ext Save Data", COLOR_TEXT, extsavedata_open},
        {"System Save Data", COLOR_TEXT, systemsavedata_open},
        {"Network Install", COLOR_TEXT, networkinstall_open},
};

static void mainmenu_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    u32 logoWidth;
    u32 logoHeight;
    screen_get_texture_size(&logoWidth, &logoHeight, TEXTURE_LOGO);

    float logoX = x1 + (x2 - x1 - logoWidth) / 2;
    float logoY = y1 + (y2 - y1 - logoHeight) / 2;
    screen_draw_texture(TEXTURE_LOGO, logoX, logoY, logoWidth, logoHeight);

    char verString[64];
    snprintf(verString, 64, "Ver. %s", VERSION_STRING);

    float verWidth;
    float verHeight;
    screen_get_string_size(&verWidth, &verHeight, verString, 0.5f, 0.5f);

    float verX = x1 + (x2 - x1 - verWidth) / 2;
    float verY = logoY + logoHeight + (y2 - (logoY + logoHeight) - verHeight) / 2;
    screen_draw_string(verString, verX, verY, 0.5f, 0.5f, COLOR_TEXT, false);
}

static void mainmenu_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_START) {
        ui_pop();
        list_destroy(view);

        return;
    }

    if(selected != NULL && (selectedTouched || hidKeysDown() & KEY_A) && selected->data != NULL) {
        ((void(*)()) selected->data)();
        return;
    }

    if(*itemCount != &mainmenu_item_count || *items != mainmenu_items) {
        *itemCount = &mainmenu_item_count;
        *items = mainmenu_items;
    }
}

void mainmenu_open() {
    list_display("Main Menu", "A: Select, START: Exit", NULL, mainmenu_update, mainmenu_draw_top);
}
