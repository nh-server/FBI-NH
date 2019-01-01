#include <stdio.h>
#include <string.h>
#include <time.h>

#include <3ds.h>
#include <malloc.h>

#include "ui.h"
#include "../error.h"
#include "../screen.h"
#include "../data/smdh.h"
#include "../../fbi/resources.h"

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
        error_panic("Failed to allocate UI view.");
        return NULL;
    }

    Result res = 0;
    if(R_FAILED(res = svcCreateEvent(&view->active, RESET_STICKY))) {
        error_panic("Failed to create view active event: 0x%08lX", res);

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
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "SD: %.1f %s",
                     ui_get_display_size(size), ui_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_CTR_NAND)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "CTR NAND: %.1f %s",
                     ui_get_display_size(size), ui_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_TWL_NAND)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "TWL NAND: %.1f %s",
                     ui_get_display_size(size), ui_get_display_size_units(size));
            currBuffer += strlen(currBuffer);
        }

        if(R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_TWL_PHOTO)) && currBuffer < ui_free_space_buffer + sizeof(ui_free_space_buffer)) {
            if(currBuffer != ui_free_space_buffer) {
                snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), ", ");
                currBuffer += strlen(currBuffer);
            }

            u64 size = (u64) resource.freeClusters * (u64) resource.clusterSize;
            snprintf(currBuffer, sizeof(ui_free_space_buffer) - (currBuffer - ui_free_space_buffer), "TWL Photo: %.1f %s",
                     ui_get_display_size(size), ui_get_display_size_units(size));
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
        float baseInfoWidth;
        screen_get_string_size(&baseInfoWidth, NULL, ui->info, 0.5f, 0.5f);

        float scale = BOTTOM_SCREEN_WIDTH / (baseInfoWidth + 10);
        if(scale > 1) {
            scale = 1;
        }

        float infoWidth;
        float infoHeight;
        screen_get_string_size(&infoWidth, &infoHeight, ui->info, 0.5f * scale, 0.5f * scale);

        screen_draw_string(ui->info, (BOTTOM_SCREEN_WIDTH - infoWidth) / 2, BOTTOM_SCREEN_HEIGHT - (bottomScreenBottomBarHeight + infoHeight) / 2, 0.5f * scale, 0.5f * scale, COLOR_TEXT, true);
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

const char* ui_get_display_eta(u32 seconds) {
    static char disp[12];

    u8 hours     = seconds / 3600;
    seconds     -= hours * 3600;
    u8 minutes   = seconds / 60;
    seconds     -= minutes* 60;

    snprintf(disp, 12, "%02u:%02u:%02u", hours, minutes, (u8) seconds);
    return disp;
}

double ui_get_display_size(u64 size) {
    double s = size;
    if(s >= 1024) {
        s /= 1024;
    }

    if(s >= 1024) {
        s /= 1024;
    }

    if(s >= 1024) {
        s /= 1024;
    }

    return s;
}

const char* ui_get_display_size_units(u64 size) {
    if(size >= 1024 * 1024 * 1024) {
        return "GiB";
    }

    if(size >= 1024 * 1024) {
        return "MiB";
    }

    if(size >= 1024) {
        return "KiB";
    }

    return "B";
}