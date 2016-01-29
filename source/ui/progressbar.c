#include <3ds.h>
#include <malloc.h>

#include "section/task.h"
#include "progressbar.h"
#include "../screen.h"

typedef struct {
    bool cancellable;
    void* data;
    float progress;
    char progressText[PROGRESS_TEXT_MAX];
    void (*update)(ui_view* view, void* data, float* progress, char* progressText);
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
} progressbar_data;

static void progressbar_update(ui_view* view, void* data, float bx1, float by1, float bx2, float by2) {
    svcWaitSynchronization(task_get_mutex(), U64_MAX);

    progressbar_data* progressBarData = (progressbar_data*) data;

    if(progressBarData->update != NULL) {
        progressBarData->update(view, progressBarData->data, &progressBarData->progress, progressBarData->progressText);
    }

    svcReleaseMutex(task_get_mutex());
}

static void progressbar_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    svcWaitSynchronization(task_get_mutex(), U64_MAX);

    progressbar_data* progressBarData = (progressbar_data*) data;

    if(progressBarData->drawTop != NULL) {
        progressBarData->drawTop(view, progressBarData->data, x1, y1, x2, y2);
    }

    svcReleaseMutex(task_get_mutex());
}

static void progressbar_draw_bottom(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    progressbar_data* progressBarData = (progressbar_data*) data;

    u32 progressBarBgWidth;
    u32 progressBarBgHeight;
    screen_get_texture_size(&progressBarBgWidth, &progressBarBgHeight, TEXTURE_PROGRESS_BAR_BG);

    float progressBarBgX = x1 + (x2 - x1 - progressBarBgWidth) / 2;
    float progressBarBgY = y1 + (y2 - y1 - progressBarBgHeight) / 2;
    screen_draw_texture(TEXTURE_PROGRESS_BAR_BG, progressBarBgX, progressBarBgY, progressBarBgWidth, progressBarBgHeight);

    u32 progressBarContentWidth;
    u32 progressBarContentHeight;
    screen_get_texture_size(&progressBarContentWidth, &progressBarContentHeight, TEXTURE_PROGRESS_BAR_CONTENT);

    float progressBarContentX = x1 + (x2 - x1 - progressBarContentWidth) / 2;
    float progressBarContentY = y1 + (y2 - y1 - progressBarContentHeight) / 2;
    screen_draw_texture_crop(TEXTURE_PROGRESS_BAR_CONTENT, progressBarContentX, progressBarContentY, progressBarContentWidth * progressBarData->progress, progressBarContentHeight);

    float progressTextWidth;
    float progressTextHeight;
    screen_get_string_size(&progressTextWidth, &progressTextHeight, progressBarData->progressText, 0.5f, 0.5f);

    float progressTextX = x1 + (x2 - x1 - progressTextWidth) / 2;
    float progressTextY = progressBarBgY + progressBarBgHeight + 10;
    screen_draw_string(progressBarData->progressText, progressTextX, progressTextY, 0.5f, 0.5f, 0xFF000000, false);
}

ui_view* progressbar_create(const char* name, const char* info, void* data, void (*update)(ui_view* view, void* data, float* progress, char* progressText),
                                                                            void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2)) {
    progressbar_data* progressBarData = (progressbar_data*) calloc(1, sizeof(progressbar_data));
    progressBarData->data = data;
    progressBarData->progress = 0;
    progressBarData->update = update;
    progressBarData->drawTop = drawTop;

    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    view->name = name;
    view->info = info;
    view->data = progressBarData;
    view->update = progressbar_update;
    view->drawTop = progressbar_draw_top;
    view->drawBottom = progressbar_draw_bottom;
    return view;
}

void progressbar_destroy(ui_view* view) {
    free(view->data);
    free(view);
}

void* progressbar_get_data(ui_view* view) {
    return ((progressbar_data*) view->data)->data;
}

char* progressbar_get_progress_text(ui_view* view) {
    return ((progressbar_data*) view->data)->progressText;
}