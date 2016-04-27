#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "list.h"
#include "mainmenu.h"
#include "ui.h"
#include "section/section.h"
#include "../core/linkedlist.h"
#include "../core/screen.h"

static list_item sd = {"SD", COLOR_TEXT, files_open_sd};
static list_item ctr_nand = {"CTR NAND", COLOR_TEXT, files_open_ctr_nand};
static list_item twl_nand = {"TWL NAND", COLOR_TEXT, files_open_twl_nand};
static list_item twl_photo = {"TWL Photo", COLOR_TEXT, files_open_twl_photo};
static list_item twl_sound = {"TWL Sound", COLOR_TEXT, files_open_twl_sound};
static list_item dump_nand = {"Dump NAND", COLOR_TEXT, dumpnand_open};
static list_item titles = {"Titles", COLOR_TEXT, titles_open};
static list_item pending_titles = {"Pending Titles", COLOR_TEXT, pendingtitles_open};
static list_item tickets = {"Tickets", COLOR_TEXT, tickets_open};
static list_item ext_save_data = {"Ext Save Data", COLOR_TEXT, extsavedata_open};
static list_item system_save_data = {"System Save Data", COLOR_TEXT, systemsavedata_open};
static list_item network_install = {"Network Install", COLOR_TEXT, networkinstall_open};
static list_item qr_code_install = {"QR Code Install", COLOR_TEXT, qrinstall_open};

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

static void mainmenu_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_START) {
        ui_pop();
        list_destroy(view);

        return;
    }

    if(selected != NULL && (selectedTouched || hidKeysDown() & KEY_A) && selected->data != NULL) {
        ((void(*)()) selected->data)();
        return;
    }

    if(linked_list_size(items) == 0) {
        linked_list_add(items, &sd);
        linked_list_add(items, &ctr_nand);
        linked_list_add(items, &twl_nand);
        linked_list_add(items, &twl_photo);
        linked_list_add(items, &twl_sound);
        linked_list_add(items, &dump_nand);
        linked_list_add(items, &titles);
        linked_list_add(items, &pending_titles);
        linked_list_add(items, &tickets);
        linked_list_add(items, &ext_save_data);
        linked_list_add(items, &system_save_data);
        linked_list_add(items, &network_install);
        linked_list_add(items, &qr_code_install);
    }
}

void mainmenu_open() {
    list_display("Main Menu", "A: Select, START: Exit", NULL, mainmenu_update, mainmenu_draw_top);
}
