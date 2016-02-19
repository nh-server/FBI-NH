#include <stdio.h>
#include <time.h>

#include <3ds.h>

#include "ui.h"
#include "../screen.h"

#define MAX_UI_VIEWS 16

static ui_view* ui_stack[MAX_UI_VIEWS];
static int ui_stack_top = -1;

bool ui_push(ui_view* view) {
    if(view == NULL) {
        return false;
    }

    if(ui_stack_top >= MAX_UI_VIEWS - 1) {
        return false;
    }

    ui_stack[++ui_stack_top] = view;
    return true;
}

ui_view* ui_peek() {
    if(ui_stack_top == -1) {
        return NULL;
    }

    return ui_stack[ui_stack_top];
}

ui_view* ui_pop() {
    if(ui_stack_top == -1) {
        return NULL;
    }

    ui_view* view = ui_peek();
    ui_stack[ui_stack_top--] = NULL;
    return view;
}

void ui_update() {
    hidScanInput();

    ui_view* ui = ui_peek();
    if(ui != NULL && ui->update != NULL) {
        u32 bottomScreenTopBarHeight = 0;
        screen_get_texture_size(NULL, &bottomScreenTopBarHeight, TEXTURE_BOTTOM_SCREEN_TOP_BAR);

        u32 bottomScreenBottomBarHeight = 0;
        screen_get_texture_size(NULL, &bottomScreenBottomBarHeight, TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR);

        ui->update(ui, ui->data, 0, bottomScreenTopBarHeight, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT - bottomScreenBottomBarHeight);
    }
}

static void ui_draw_top(ui_view* ui) {
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

    screen_select(GFX_TOP);
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

    time_t t = time(NULL);
    char* timeText = ctime(&t);

    float timeTextWidth;
    float timeTextHeight;
    screen_get_string_size(&timeTextWidth, &timeTextHeight, timeText, 0.5f, 0.5f);
    screen_draw_string(timeText, topScreenTopBarX + (topScreenTopBarWidth - timeTextWidth) / 2, topScreenTopBarY + (topScreenTopBarHeight - timeTextHeight) / 2, 0.5f, 0.5f, 0xFF000000, false);

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

    FS_ArchiveResource sd;
    FSUSER_GetSdmcArchiveResource(&sd);

    FS_ArchiveResource nand;
    FSUSER_GetNandArchiveResource(&nand);

    char buffer[64];
    snprintf(buffer, 64, "SD: %.1f MiB, NAND: %.1f MiB", ((u64) sd.freeClusters * (u64) sd.clusterSize) / 1024.0 / 1024.0, ((u64) nand.freeClusters * (u64) nand.clusterSize) / 1024.0 / 1024.0);

    float freeSpaceHeight;
    screen_get_string_size(NULL, &freeSpaceHeight, buffer, 0.5f, 0.5f);

    screen_draw_string(buffer, topScreenBottomBarX + 2, topScreenBottomBarY + (topScreenBottomBarHeight - freeSpaceHeight) / 2, 0.5f, 0.5f, 0xFF000000, false);
}

static void ui_draw_bottom(ui_view* ui) {
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

    screen_select(GFX_BOTTOM);
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_BG, (BOTTOM_SCREEN_WIDTH - bottomScreenBgWidth) / 2, (BOTTOM_SCREEN_HEIGHT - bottomScreenBgHeight) / 2, bottomScreenBgWidth, bottomScreenBgHeight);

    if(ui->drawBottom != NULL) {
        ui->drawBottom(ui, ui->data, 0, bottomScreenTopBarHeight, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT - bottomScreenBottomBarHeight);
    }

    float bottomScreenTopBarX = (BOTTOM_SCREEN_WIDTH - bottomScreenTopBarWidth) / 2;
    float bottomScreenTopBarY = 0;
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_TOP_BAR, bottomScreenTopBarX, bottomScreenTopBarY, bottomScreenTopBarWidth, bottomScreenTopBarHeight);
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_TOP_BAR_SHADOW, bottomScreenTopBarX, bottomScreenTopBarY + bottomScreenTopBarHeight, bottomScreenTopBarShadowWidth, bottomScreenTopBarShadowHeight);

    float bottomScreenBottomBarX = (BOTTOM_SCREEN_WIDTH - bottomScreenBottomBarWidth) / 2;
    float bottomScreenBottomBarY = BOTTOM_SCREEN_HEIGHT - bottomScreenBottomBarHeight;
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR, bottomScreenBottomBarX, bottomScreenBottomBarY, bottomScreenBottomBarWidth, bottomScreenBottomBarHeight);
    screen_draw_texture(TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR_SHADOW, bottomScreenBottomBarX, bottomScreenBottomBarY - bottomScreenBottomBarShadowHeight, bottomScreenBottomBarShadowWidth, bottomScreenBottomBarShadowHeight);

    if(ui->name != NULL) {
        float nameWidth;
        float nameHeight;
        screen_get_string_size(&nameWidth, &nameHeight, ui->name, 0.5f, 0.5f);
        screen_draw_string(ui->name, (BOTTOM_SCREEN_WIDTH - nameWidth) / 2, (bottomScreenTopBarHeight - nameHeight) / 2, 0.5f, 0.5f, 0xFF000000, false);
    }

    if(ui->info != NULL) {
        float infoWidth;
        float infoHeight;
        screen_get_string_size(&infoWidth, &infoHeight, ui->info, 0.5f, 0.5f);
        screen_draw_string(ui->info, (BOTTOM_SCREEN_WIDTH - infoWidth) / 2, BOTTOM_SCREEN_HEIGHT - (bottomScreenBottomBarHeight + infoHeight) / 2, 0.5f, 0.5f, 0xFF000000, false);
    }
}

void ui_draw() {
    ui_view* ui = ui_peek();
    if(ui != NULL) {
        screen_begin_frame();
        ui_draw_top(ui);
        ui_draw_bottom(ui);
        screen_end_frame();
    }
}
