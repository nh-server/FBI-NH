#include <malloc.h>

#include <3ds.h>

#include "error.h"
#include "prompt.h"
#include "ui.h"
#include "../core/screen.h"

typedef struct {
    const char* text;
    u32 rgba;
    bool option;
    void* data;
    void (*update)(ui_view* view, void* data);
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
    void (*onResponse)(ui_view* view, void* data, bool response);
} prompt_data;

static void prompt_notify_response(ui_view* view, prompt_data* promptData, bool response) {
    ui_pop();

    if(promptData->onResponse != NULL) {
        promptData->onResponse(view, promptData->data, response);
    }

    free(promptData);
    free(view);
}

static void prompt_update(ui_view* view, void* data, float bx1, float by1, float bx2, float by2) {
    prompt_data* promptData = (prompt_data*) data;

    if(!promptData->option && (hidKeysDown() & ~KEY_TOUCH)) {
        prompt_notify_response(view, promptData, false);
        return;
    }

    if(promptData->option && (hidKeysDown() & (KEY_A | KEY_B))) {
        prompt_notify_response(view, promptData, (bool) (hidKeysDown() & KEY_A));
        return;
    }

    if(hidKeysDown() & KEY_TOUCH) {
        touchPosition pos;
        hidTouchRead(&pos);

        if(promptData->option) {
            u32 buttonWidth;
            u32 buttonHeight;
            screen_get_texture_size(&buttonWidth, &buttonHeight, TEXTURE_BUTTON_SMALL);

            float yesButtonX = bx1 + (bx2 - bx1) / 2 - 5 - buttonWidth;
            float yesButtonY = by2 - 5 - buttonHeight;
            if(pos.px >= yesButtonX && pos.py >= yesButtonY && pos.px < yesButtonX + buttonWidth && pos.py < yesButtonY + buttonHeight) {
                prompt_notify_response(view, promptData, true);
                return;
            }

            float noButtonX = bx1 + (bx2 - bx1) / 2 + 5;
            float noButtonY = by2 - 5 - buttonHeight;
            if(pos.px >= noButtonX && pos.py >= noButtonY && pos.px < noButtonX + buttonWidth && pos.py < noButtonY + buttonHeight) {
                prompt_notify_response(view, promptData, false);
                return;
            }
        } else {
            u32 buttonWidth;
            u32 buttonHeight;
            screen_get_texture_size(&buttonWidth, &buttonHeight, TEXTURE_BUTTON_LARGE);

            float okayButtonX = bx1 + (bx2 - bx1 - buttonWidth) / 2;
            float okayButtonY = by2 - 5 - buttonHeight;
            if(pos.px >= okayButtonX && pos.py >= okayButtonY && pos.px < okayButtonX + buttonWidth && pos.py < okayButtonY + buttonHeight) {
                prompt_notify_response(view, promptData, false);
                return;
            }
        }
    }

    if(promptData->update != NULL) {
        promptData->update(view, promptData->data);
    }
}

static void prompt_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    prompt_data* promptData = (prompt_data*) data;

    if(promptData->drawTop != NULL) {
        promptData->drawTop(view, promptData->data, x1, y1, x2, y2);
    }
}

static void prompt_draw_bottom(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    prompt_data* promptData = (prompt_data*) data;

    u32 buttonWidth;
    u32 buttonHeight;
    if(promptData->option) {
        screen_get_texture_size(&buttonWidth, &buttonHeight, TEXTURE_BUTTON_SMALL);

        float yesButtonX = x1 + (x2 - x1) / 2 - 5 - buttonWidth;
        float yesButtonY = y2 - 5 - buttonHeight;
        screen_draw_texture(TEXTURE_BUTTON_SMALL, yesButtonX, yesButtonY, buttonWidth, buttonHeight);

        float noButtonX = x1 + (x2 - x1) / 2 + 5;
        float noButtonY = y2 - 5 - buttonHeight;
        screen_draw_texture(TEXTURE_BUTTON_SMALL, noButtonX, noButtonY, buttonWidth, buttonHeight);

        static const char* yes = "Yes (A)";
        static const char* no = "No (B)";

        float yesWidth;
        float yesHeight;
        screen_get_string_size(&yesWidth, &yesHeight, yes, 0.5f, 0.5f);
        screen_draw_string(yes, yesButtonX + (buttonWidth - yesWidth) / 2, yesButtonY + (buttonHeight - yesHeight) / 2, 0.5f, 0.5f, promptData->rgba, false);

        float noWidth;
        float noHeight;
        screen_get_string_size(&noWidth, &noHeight, no, 0.5f, 0.5f);
        screen_draw_string(no, noButtonX + (buttonWidth - noWidth) / 2, noButtonY + (buttonHeight - noHeight) / 2, 0.5f, 0.5f, promptData->rgba, false);
    } else {
        screen_get_texture_size(&buttonWidth, &buttonHeight, TEXTURE_BUTTON_LARGE);

        float okayButtonX = x1 + (x2 - x1 - buttonWidth) / 2;
        float okayButtonY = y2 - 5 - buttonHeight;
        screen_draw_texture(TEXTURE_BUTTON_LARGE, okayButtonX, okayButtonY, buttonWidth, buttonHeight);

        static const char* okay = "Okay (Any Button)";

        float okayWidth;
        float okayHeight;
        screen_get_string_size(&okayWidth, &okayHeight, okay, 0.5f, 0.5f);
        screen_draw_string(okay, okayButtonX + (buttonWidth - okayWidth) / 2, okayButtonY + (buttonHeight - okayHeight) / 2, 0.5f, 0.5f, promptData->rgba, false);
    }

    float textWidth;
    float textHeight;
    screen_get_string_size(&textWidth, &textHeight, promptData->text, 0.5f, 0.5f);
    screen_draw_string(promptData->text, x1 + (x2 - x1 - textWidth) / 2, y1 + (y2 - 5 - buttonHeight - y1 - textHeight) / 2, 0.5f, 0.5f, promptData->rgba, false);
}

void prompt_display(const char* name, const char* text, u32 rgba, bool option, void* data, void (*update)(ui_view* view, void* data),
                                                                                           void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2),
                                                                                           void (*onResponse)(ui_view* view, void* data, bool response)) {
    prompt_data* promptData = (prompt_data*) calloc(1, sizeof(prompt_data));
    if(promptData == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate prompt data.");

        return;
    }

    promptData->text = text;
    promptData->rgba = rgba;
    promptData->option = option;
    promptData->data = data;
    promptData->update = update;
    promptData->drawTop = drawTop;
    promptData->onResponse = onResponse;

    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    if(view == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate UI view.");

        free(promptData);
        return;
    }

    view->name = name;
    view->info = "";
    view->data = promptData;
    view->update = prompt_update;
    view->drawTop = prompt_draw_top;
    view->drawBottom = prompt_draw_bottom;
    ui_push(view);
}