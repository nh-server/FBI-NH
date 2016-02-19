#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "list.h"
#include "mainmenu.h"
#include "section/section.h"
#include "../screen.h"

#define MAINMENU_ITEM_COUNT 10

static u32 mainmenu_item_count = MAINMENU_ITEM_COUNT;
static list_item mainmenu_items[MAINMENU_ITEM_COUNT] = {
        {"SD", 0xFF000000, files_open_sd},
        {"CTR NAND", 0xFF000000, files_open_ctrnand},
        {"TWL NAND", 0xFF000000, files_open_twlnand},
        {"Titles", 0xFF000000, titles_open},
        {"Pending Titles", 0xFF000000, pendingtitles_open},
        {"Tickets", 0xFF000000, tickets_open},
        {"Ext Save Data", 0xFF000000, extsavedata_open},
        {"System Save Data", 0xFF000000, systemsavedata_open},
        {"Network Install to SD", 0xFF000000, networkinstall_open_sd},
        {"Network Install to NAND", 0xFF000000, networkinstall_open_nand},
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
    screen_draw_string(verString, verX, verY, 0.5f, 0.5f, 0xFF000000, false);
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
    ui_push(list_create("Main Menu", "A: Select, START: Exit", NULL, mainmenu_update, mainmenu_draw_top));
}
