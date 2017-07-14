#include <stdio.h>
#include <string.h>
#include <time.h>

#include <3ds.h>
#include <malloc.h>

#include "ui.h"
#include "section/task/task.h"
#include "../core/screen.h"
#include "../core/util.h"

#define MAX_UI_VIEWS 16

static ui_view* ui_stack[MAX_UI_VIEWS];
static int ui_stack_top = -1;

static Handle ui_stack_mutex = 0;

static u64 ui_free_space_last_update = 0;
static char ui_free_space_buffer[128];

static u64 ui_fade_begin_time = 0;
static u8 ui_fade_alpha = 0;

void ui_init() {
    if(ui_stack_mutex == 0) {
        svcCreateMutex(&ui_stack_mutex, false);
    }

    ui_fade_begin_time = osGetTime();
}

void ui_exit() {
    if(ui_stack_mutex != 0) {
        svcCloseHandle(ui_stack_mutex);
        ui_stack_mutex = 0;
    }
}

ui_view* ui_create() {
    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    if(view == NULL) {
        util_panic("Failed to allocate UI view.");
        return NULL;
    }

    Result res = 0;
    if(R_FAILED(res = svcCreateEvent(&view->active, RESET_STICKY))) {
        util_panic("Failed to create view active event: 0x%08lX", res);

        free(view);
        return NULL;
    }

    return view;
}

void ui_destroy(ui_view* view) {
    if(view != NULL) {
        svcCloseHandle(view->active);
        free(view);
    }
}

ui_view* ui_top() {
    svcWaitSynchronization(ui_stack_mutex, U64_MAX);

    ui_view* ui = NULL;
    if(ui_stack_top >= 0) {
        ui = ui_stack[ui_stack_top];
    }

    svcReleaseMutex(ui_stack_mutex);

    return ui;
}

bool ui_push(ui_view* view) {
    if(view == NULL) {
        return false;
    }

    svcWaitSynchronization(ui_stack_mutex, U64_MAX);

    bool space = ui_stack_top < MAX_UI_VIEWS - 1;
    if(space) {
        ui_stack[++ui_stack_top] = view;

        svcClearEvent(view->active);
    }

    svcReleaseMutex(ui_stack_mutex);

    return space;
}

void ui_pop() {
    svcWaitSynchronization(ui_stack_mutex, U64_MAX);

    if(ui_stack_top >= 0) {
        svcSignalEvent(ui_stack[ui_stack_top]->active);

        ui_stack[ui_stack_top--] = NULL;
    }

    svcReleaseMutex(ui_stack_mutex);
}

static void ui_draw_top(ui_view* ui) {
    screen_select(GFX_TOP);

    u32 topScreenBgWidth = 0;
    u32 topScreenBgHeight = 0;
    screen_get_texture_size(&topScreenBgWidth, &topScreenBgHeight, TEXTURE_TOP_SCREEN_BG);

    u32 topScreenTopBarWidth = 0;
    u32 topScreenTopBarHeight = 0;
    screen_get_texture_size(&topScreenTopBarWidth, &topScreenTopBarHeight, TEXTURE_TOP_SCREEN_TOP_BAR);

    u32 topScreenTopBarShadowWidth = 0;
    u32 topScreenTopBarShadowHeight = 0;
    screen_get_texture_size(&topScreenTopBarShadowWidth, &topScreenTopBarShadowHeight, TEXTURE_TOP_SCREEN_TOP_BAR_SHADOW);

    u32 topScreenBottomBarWidth = 0;
    u32 topScreenBottomBarHeight = 0;
    screen_get_texture_size(&topScreenBottomBarWidth, &topScreenBottomBarHeight, TEXTURE_TOP_SCREEN_BOTTOM_BAR);

    u32 topScreenBottomBarShadowWidth = 0;
    u32 topScreenBottomBarShadowHeight = 0;
    screen_get_texture_size(&topScreenBottomBarShadowWidth, &topScreenBottomBarShadowHeight, TEXTURE_TOP_SCREEN_BOTTOM_BAR_SHADOW);

    screen_draw_texture(TEXTURE_TOP_SCREEN_BG, (TOP_SCREEN_WIDTH - topScreenBgWidth) / 2, (TOP_SCREEN_HEIGHT - topScreenBgHeight) / 2, topScreenBgWidth, topScreenBgHeight);

    if(ui->drawTop != NULL) {
        ui->drawTop(ui, ui->data, 0, topScreenTopBarHeight, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT - topScreenBottomBarHeight);
    }

    float topScreenTopBarX = (TOP_SCREEN_WIDTH - topScreenTopBarWidth) / 2;
    float topScreenTopBarY = 0;
    screen_draw_texture(TEXTURE_TOP_SCREEN_TOP_BAR, topScreenTopBarX, topScreenTopBarY, topScreenTopBarWidth, topScreenTopBarHeight);
    screen_draw_texture(TEXTURE_TOP_SCREEN_TOP_BAR_SHADOW, topScreenTopBarX, topScreenTopBarY + topScreenTopBarHeight, topScreenTopBarShadowWidth, topScreenTopBarShadowHeight);

    float topScreenBottomBarX = (TOP_SCREEN_WIDTH - topScreenBottomBarWidth) / 2;
    float topScreenBottomBarY = TOP_SCREEN_HEIGHT - topScreenBottomBarHeight;
    screen_draw_texture(TEXTURE_TOP_SCREEN_BOTTOM_BAR, topScreenBottomBarX, topScreenBottomBarY, topScreenBottomBarWidth, topScreenBottomBarHeight);
    screen_draw_texture(TEXTURE_TOP_SCREEN_BOTTOM_BAR_SHADOW, topScreenBottomBarX, topScreenBottomBarY - topScreenBottomBarShadowHeight, topScreenBottomBarShadowWidth, topScreenBottomBarShadowHeight);

    screen_set_base_alpha(ui_fade_alpha);

    char verText[64];
    snprintf(verText, 64, "Ver. %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

    float verWidth;
    float verHeight;
    screen_get_string_size(&verWidth, &verHeight, verText, 0.5f, 0.5f);
    screen_draw_string(verText, topScreenTopBarX + 2, topScreenTopBarY + (topScreenTopBarHeight - verHeight) / 2, 0.5f, 0.5f, COLOR_TEXT, true);

    time_t t = time(NULL);
    char* timeText = ctime(&t);
    timeText[strlen(timeText) - 1] = '\0';

    float timeTextWidth;
    float timeTextHeight;
    screen_get_string_size(&timeTextWidth, &timeTextHeight, timeText, 0.5f, 0.5f);
    screen_draw_string(timeText, topScreenTopBarX + (topScreenTopBarWidth - timeTextWidth) / 2, topScreenTopBarY + (topScreenTopBarHeight - timeTextHeight) / 2, 0.5f, 0.5f, COLOR_TEXT, true);

    u32 batteryIcon = 0;
    u8 batteryChargeState = 0;
    u8 batteryLevel = 0;
    if(R_SUCCEEDED(PTMU_GetBatteryChargeState(&batteryChargeState)) && batteryChargeState) {
        batteryIcon = TEXTURE_BATTERY_CHARGING;
    } else if(R_SUCCEEDED(PTMU_GetBatteryLevel(&batteryLevel))) {
        batteryIcon = TEXTURE_BATTERY_0 + batteryLevel;
    } else {
        batteryIcon = TEXTURE_BATTERY_0;
    }

    u32 batteryWidth;
    u32 batteryHeight;
    screen_get_texture_size(&batteryWidth, &batteryHeight, batteryIcon);

    float batteryX = topScreenTopBarX + topScreenTopBarWidth - 2 - batteryWidth;
    float batteryY = topScreenTopBarY + (topScreenTopBarHeight - batteryHeight) / 2;
    screen_draw_texture(batteryIcon, batteryX, batteryY, batteryWidth, batteryHeight);

    u32 wifiIcon = 0;
    u32 wifiStatus = 0;
    if(R_SUCCEEDED(ACU_GetWifiStatus(&wifiStatus)) && wifiStatus) {
        wifiIcon = TEXTURE_WIFI_0 + osGetWifiStrength();
    } else {
        wifiIcon = TEXTURE_WIFI_DISCONNECTED;
    }

    u32 wifiWidth;
    u32 wifiHeight;
    screen_get_texture_size(&wifiWidth, &wifiHeight, wifiIcon);

    float wifiX = topScreenTopBarX + topScreenTopBarWidth - 2 - batteryWidth - 4 - wifiWidth;
    float wifiY = topScreenTopBarY + (topScreenTopBarHeight - wifiHeight) / 2;
    screen_draw_texture(wifiIcon, wifiX, wifiY, wifiWidth, wifiHeight);

    if(osGetTime() >= ui_free_space_last_update + 1000) {
        char* currBuffer = ui_free_space_buffer;
        FS_ArchiveResource resource = {0};

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_SD)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "SD: %.1f %s", util_get_display_size(size), util_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_CTR_NAND)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "CTR NAND: %.1f %s", util_get_display_size(size), util_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_TWL_NAND)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "TWL NAND: %.1f %s", util_get_display_size(size), util_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_TWL_PHOTO)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "TWL Photo: %.1f %s", util_get_display_size(size), util_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        ui_free_space_last_update = osGetTime();
    }

    float freeSpaceHeight;
    screen_get_string_size(NULL, &freeSpaceHeight, ui_free_space_buffer, 0.35f, 0.35f);

    screen_draw_string(ui_free_space_buffer, topScreenBottomBarX + 2, topScreenBottomBarY + (topScreenBottomBarHeight - freeSpaceHeight) / 2, 0.35f, 0.35f, COLOR_TEXT, true);

    screen_set_base_alpha(0xFF);
}

static void ui_draw_bottom(ui_view* ui) {
    screen_select(GFX_BOTTOM);

    u32 bottomScreenBgWidth = 0;
    u32 bottomScreenBgHeight = 0;
    screen_get_texture_size(&bottomScreenBgWidth, &bottomScreenBgHeight, TEXTURE_BOTTOM_SCREEN_BG);

    u32 bottomScreenTopBarWidth = 0;
    u32 bottomScreenTopBarHeight = 0;
    screen_get_texture_size(&bottomScreenTopBarWidth, &bottomScreenTopBarHeight, TEXTURE_BOTTOM_SCREEN_TOP_BAR);

    u32 bottomScreenTopBarShadowWidth = 0;
    u32 bottomScreenTopBarShadowHeight = 0;
    screen_get_texture_size(&bottomScreenTopBarShadowWidth, &bottomScreenTopBarShadowHeight, TEXTURE_BOTTOM_SCREEN_TOP_BAR_SHADOW);

    u32 bottomScreenBottomBarWidth = 0;
    u32 bottomScreenBottomBarHeight = 0;
    screen_get_texture_size(&bottomScreenBottomBarWidth, &bottomScreenBottomBarHeight, TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR);

    u32 bottomScreenBottomBarShadowWidth = 0;
    u32 bottomScreenBottomBarShadowHeight = 0;
    screen_get_texture_size(&bottomScreenBottomBarShadowWidth, &bottomScreenBottomBarShadowHeight, TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR_SHADOW);

    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_BG, (BOTTOM_SCREEN_WIDTH - bottomScreenBgWidth) / 2, (BOTTOM_SCREEN_HEIGHT - bottomScreenBgHeight) / 2, bottomScreenBgWidth, bottomScreenBgHeight);

    screen_set_base_alpha(ui_fade_alpha);

    if(ui->drawBottom != NULL) {
        ui->drawBottom(ui, ui->data, 0, bottomScreenTopBarHeight, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT - bottomScreenBottomBarHeight);
    }

    screen_set_base_alpha(0xFF);

    float bottomScreenTopBarX = (BOTTOM_SCREEN_WIDTH - bottomScreenTopBarWidth) / 2;
    float bottomScreenTopBarY = 0;
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_TOP_BAR, bottomScreenTopBarX, bottomScreenTopBarY, bottomScreenTopBarWidth, bottomScreenTopBarHeight);
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_TOP_BAR_SHADOW, bottomScreenTopBarX, bottomScreenTopBarY + bottomScreenTopBarHeight, bottomScreenTopBarShadowWidth, bottomScreenTopBarShadowHeight);

    float bottomScreenBottomBarX = (BOTTOM_SCREEN_WIDTH - bottomScreenBottomBarWidth) / 2;
    float bottomScreenBottomBarY = BOTTOM_SCREEN_HEIGHT - bottomScreenBottomBarHeight;
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR, bottomScreenBottomBarX, bottomScreenBottomBarY, bottomScreenBottomBarWidth, bottomScreenBottomBarHeight);
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR_SHADOW, bottomScreenBottomBarX, bottomScreenBottomBarY - bottomScreenBottomBarShadowHeight, bottomScreenBottomBarShadowWidth, bottomScreenBottomBarShadowHeight);

    screen_set_base_alpha(ui_fade_alpha);

    if(ui->name != NULL) {
        float nameWidth;
        float nameHeight;
        screen_get_string_size(&nameWidth, &nameHeight, ui->name, 0.5f, 0.5f);
        screen_draw_string(ui->name, (BOTTOM_SCREEN_WIDTH - nameWidth) / 2, (bottomScreenTopBarHeight - nameHeight) / 2, 0.5f, 0.5f, COLOR_TEXT, true);
    }

    if(ui->info != NULL) {
        float infoWidth;
        float infoHeight;
        screen_get_string_size(&infoWidth, &infoHeight, ui->info, 0.5f, 0.5f);
        screen_draw_string(ui->info, (BOTTOM_SCREEN_WIDTH - infoWidth) / 2, BOTTOM_SCREEN_HEIGHT - (bottomScreenBottomBarHeight + infoHeight) / 2, 0.5f, 0.5f, COLOR_TEXT, true);
    }

    screen_set_base_alpha(0xFF);
}

bool ui_update() {
    ui_view* ui = NULL;

    hidScanInput();

    ui = ui_top();
    if(ui != NULL && ui->update != NULL) {
        u32 bottomScreenTopBarHeight = 0;
        screen_get_texture_size(NULL, &bottomScreenTopBarHeight, TEXTURE_BOTTOM_SCREEN_TOP_BAR);

        u32 bottomScreenBottomBarHeight = 0;
        screen_get_texture_size(NULL, &bottomScreenBottomBarHeight, TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR);

        ui->update(ui, ui->data, 0, bottomScreenTopBarHeight, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT - bottomScreenBottomBarHeight);
    }

    u64 time = osGetTime();
    if(!envIsHomebrew() && time - ui_fade_begin_time < 500) {
        ui_fade_alpha = (u8) (((time - ui_fade_begin_time) / 500.0f) * 0xFF);
    } else {
        ui_fade_alpha = 0xFF;
    }

    ui = ui_top();
    if(ui != NULL) {
        screen_begin_frame();
        ui_draw_top(ui);
        ui_draw_bottom(ui);
        screen_end_frame();
    }

    return ui != NULL;
}

void ui_draw_meta_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    meta_info* info = (meta_info*) data;

    u32 metaInfoBoxShadowWidth;
    u32 metaInfoBoxShadowHeight;
    screen_get_texture_size(&metaInfoBoxShadowWidth, &metaInfoBoxShadowHeight, TEXTURE_META_INFO_BOX_SHADOW);

    float metaInfoBoxShadowX = x1 + (x2 - x1 - metaInfoBoxShadowWidth) / 2;
    float metaInfoBoxShadowY = y1 + (y2 - y1) / 4 - metaInfoBoxShadowHeight / 2;
    screen_draw_texture(TEXTURE_META_INFO_BOX_SHADOW, metaInfoBoxShadowX, metaInfoBoxShadowY, metaInfoBoxShadowWidth, metaInfoBoxShadowHeight);

    u32 metaInfoBoxWidth;
    u32 metaInfoBoxHeight;
    screen_get_texture_size(&metaInfoBoxWidth, &metaInfoBoxHeight, TEXTURE_META_INFO_BOX);

    float metaInfoBoxX = x1 + (x2 - x1 - metaInfoBoxWidth) / 2;
    float metaInfoBoxY = y1 + (y2 - y1) / 4 - metaInfoBoxHeight / 2;
    screen_draw_texture(TEXTURE_META_INFO_BOX, metaInfoBoxX, metaInfoBoxY, metaInfoBoxWidth, metaInfoBoxHeight);

    if(info->texture != 0) {
        u32 iconWidth;
        u32 iconHeight;
        screen_get_texture_size(&iconWidth, &iconHeight, info->texture);

        float iconX = metaInfoBoxX + (64 - iconWidth) / 2;
        float iconY = metaInfoBoxY + (metaInfoBoxHeight - iconHeight) / 2;
        screen_draw_texture(info->texture, iconX, iconY, iconWidth, iconHeight);
    }

    float metaTextX = metaInfoBoxX + 64;

    float shortDescriptionHeight;
    screen_get_string_size_wrap(NULL, &shortDescriptionHeight, info->shortDescription, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float longDescriptionHeight;
    screen_get_string_size_wrap(NULL, &longDescriptionHeight, info->longDescription, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float publisherHeight;
    screen_get_string_size_wrap(NULL, &publisherHeight, info->publisher, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float shortDescriptionY = metaInfoBoxY + (64 - shortDescriptionHeight - 2 - longDescriptionHeight - 2 - publisherHeight) / 2;
    screen_draw_string_wrap(info->shortDescription, metaTextX, shortDescriptionY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);

    float longDescriptionY = shortDescriptionY + shortDescriptionHeight + 2;
    screen_draw_string_wrap(info->longDescription, metaTextX, longDescriptionY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);

    float publisherY = longDescriptionY + longDescriptionHeight + 2;
    screen_draw_string_wrap(info->publisher, metaTextX, publisherY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);
}

void ui_draw_ext_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ext_save_data_info* info = (ext_save_data_info*) data;

    if(info->hasMeta) {
        ui_draw_meta_info(view, &info->meta, x1, y1, x2, y2);
    }

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             "Ext Save Data ID: %016llX\n"
             "Shared: %s",
             info->extSaveDataId,
             info->shared ? "Yes" : "No");

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void ui_draw_file_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    file_info* info = (file_info*) data;

    char infoText[512];
    size_t infoTextPos = 0;

    if(strlen(info->name) > 48) {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Name: %.45s...\n", info->name);
    } else {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Name: %.48s\n", info->name);
    }

    infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Attributes: ");

    if(info->attributes & (FS_ATTRIBUTE_DIRECTORY | FS_ATTRIBUTE_HIDDEN | FS_ATTRIBUTE_ARCHIVE | FS_ATTRIBUTE_READ_ONLY)) {
        bool needsSeparator = false;

        if(info->attributes & FS_ATTRIBUTE_DIRECTORY) {
            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Directory");
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_HIDDEN) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Hidden");
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_ARCHIVE) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Archive");
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_READ_ONLY) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Read Only");
            needsSeparator = true;
        }
    } else {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "None");
    }

    infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "\n");

    if(!(info->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Size: %.2f %s\n", util_get_display_size(info->size), util_get_display_size_units(info->size));

        if(info->isCia) {
            char regionString[64];

            if(info->ciaInfo.hasMeta) {
                ui_draw_meta_info(view, &info->ciaInfo.meta, x1, y1, x2, y2);

                util_smdh_region_to_string(regionString, info->ciaInfo.meta.region, sizeof(regionString));
            } else {
                snprintf(regionString, sizeof(regionString), "Unknown");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos,
                     "Title ID: %016llX\n"
                     "Version: %hu (%d.%d.%d)\n"
                     "Region: %s\n"
                     "Installed Size: %.2f %s",
                     info->ciaInfo.titleId,
                     info->ciaInfo.version, (info->ciaInfo.version >> 10) & 0x3F, (info->ciaInfo.version >> 4) & 0x3F, info->ciaInfo.version & 0xF,
                     regionString,
                     util_get_display_size(info->ciaInfo.installedSize), util_get_display_size_units(info->ciaInfo.installedSize));
        } else if(info->isTicket) {
            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Ticket ID: %016llX", info->ticketInfo.titleId);
        }
    }

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void ui_draw_pending_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    pending_title_info* info = (pending_title_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             "Pending Title ID: %016llX\n"
             "Media Type: %s\n"
             "Version: %hu (%d.%d.%d)",
             info->titleId,
             info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? "SD" : "Game Card",
             info->version, (info->version >> 10) & 0x3F, (info->version >> 4) & 0x3F, info->version & 0xF);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void ui_draw_system_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    system_save_data_info* info = (system_save_data_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText), "System Save Data ID: %08lX", info->systemSaveDataId);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void ui_draw_ticket_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ticket_info* info = (ticket_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText), "Title ID: %016llX", info->titleId);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void ui_draw_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    title_info* info = (title_info*) data;

    char regionString[64];

    if(info->hasMeta) {
        ui_draw_meta_info(view, &info->meta, x1, y1, x2, y2);

        util_smdh_region_to_string(regionString, info->meta.region, sizeof(regionString));
    } else {
        snprintf(regionString, sizeof(regionString), "Unknown");
    }

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             "Title ID: %016llX\n"
             "Media Type: %s\n"
             "Version: %hu\n"
             "Product Code: %s\n"
             "Region: %s\n"
             "Size: %.2f %s",
             info->titleId,
             info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? "SD" : "Game Card",
             info->version,
             info->productCode,
             regionString,
             util_get_display_size(info->installedSize), util_get_display_size_units(info->installedSize));

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void ui_draw_titledb_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    titledb_info* info = (titledb_info*) data;

    ui_draw_meta_info(view, &info->meta, x1, y1, x2, y2);

    char updatedDate[32];
    char updatedTime[32];

    sscanf(info->updatedAt, "%[^T]T%[^Z]Z", updatedDate, updatedTime);

    char infoText[512];

    // TODO: Latest version disabled pending TitleDB pull request.
    snprintf(infoText, sizeof(infoText),
             "Title ID: %016llX\n"
             "Installed Version: %hu (%d.%d.%d)\n"
             //"Latest Version: %hu (%d.%d.%d)\n"
             "Size: %.2f %s\n"
             "Updated At: %s %s",
             info->titleId,
             info->installedVersion, (info->installedVersion >> 10) & 0x3F, (info->installedVersion >> 4) & 0x3F, info->installedVersion & 0xF,
             //info->latestVersion, (info->latestVersion >> 10) & 0x3F, (info->latestVersion >> 4) & 0x3F, info->latestVersion & 0xF,
             util_get_display_size(info->size), util_get_display_size_units(info->size),
             updatedDate, updatedTime);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}
