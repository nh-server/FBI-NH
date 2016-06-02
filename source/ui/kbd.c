#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "error.h"
#include "ui.h"
#include "../core/screen.h"

#define MAX_INPUT_SIZE 1024

typedef struct {
    char input[MAX_INPUT_SIZE];
    size_t inputPos;

    bool shift;
    bool capsLock;

    float scrollPos;
    u32 lastTouchY;

    bool keyboardTouched;
    u64 nextRepeatTime;

    void* data;
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
    void (*finished)(void* data, char* input);
    void (*canceled)(void* data);
} kbd_data;

typedef struct {
    char normal;
    char special;

    void (*press)(ui_view* view, kbd_data* data);

    u32 x;
    u32 y;
    u32 width;
    u32 height;
} LayoutKey;

static void kbd_backspace(ui_view* view, kbd_data* data) {
    if(data->inputPos > 0) {
        data->input[--data->inputPos] = '\0';
    }
}

static void kbd_capslock(ui_view* view, kbd_data* data) {
    data->capsLock = !data->capsLock;
}

static void kbd_shift(ui_view* view, kbd_data* data) {
    data->shift = !data->shift;
}

static void kbd_cancel(ui_view* view, kbd_data* data) {
    ui_pop();

    if(data->canceled != NULL) {
        data->canceled(data->data);
    }

    free(data);
    free(view);
}

static void kbd_finish(ui_view* view, kbd_data* data) {
    ui_pop();

    if(data->finished != NULL) {
        data->finished(data->data, data->input);
    }

    free(data);
    free(view);
}

#define NUM_KEYS 56

static LayoutKey layout[NUM_KEYS] = {
        // Row 1
        {'`',  '~',  NULL,          1,   1, 21, 21},
        {'1',  '!',  NULL,          22,  1, 21, 21},
        {'2',  '@',  NULL,          43,  1, 21, 21},
        {'3',  '#',  NULL,          64,  1, 21, 21},
        {'4',  '$',  NULL,          85,  1, 21, 21},
        {'5',  '%',  NULL,          106, 1, 21, 21},
        {'6',  '^',  NULL,          127, 1, 21, 21},
        {'7',  '&',  NULL,          148, 1, 21, 21},
        {'8',  '*',  NULL,          169, 1, 21, 21},
        {'9',  '(',  NULL,          190, 1, 21, 21},
        {'0',  ')',  NULL,          211, 1, 21, 21},
        {'-',  '_',  NULL,          232, 1, 21, 21},
        {'=',  '+',  NULL,          253, 1, 21, 21},
        {'\0', '\0', kbd_backspace, 274, 1, 42, 21},

        // Row 2
        {'\t', '\t', NULL, 1,   22, 31, 21},
        {'q',  'Q',  NULL, 32,  22, 21, 21},
        {'w',  'W',  NULL, 53,  22, 21, 21},
        {'e',  'E',  NULL, 74,  22, 21, 21},
        {'r',  'R',  NULL, 95,  22, 21, 21},
        {'t',  'T',  NULL, 116, 22, 21, 21},
        {'y',  'Y',  NULL, 137, 22, 21, 21},
        {'u',  'U',  NULL, 158, 22, 21, 21},
        {'i',  'I',  NULL, 179, 22, 21, 21},
        {'o',  'O',  NULL, 200, 22, 21, 21},
        {'p',  'P',  NULL, 221, 22, 21, 21},
        {'[',  '{',  NULL, 242, 22, 21, 21},
        {']',  '}',  NULL, 263, 22, 21, 21},
        {'\\', '|',  NULL, 284, 22, 32, 21},

        // Row 3
        {'\0', '\0', kbd_capslock, 1,   43, 36, 21},
        {'a',  'A',  NULL,         37,  43, 21, 21},
        {'s',  'S',  NULL,         58,  43, 21, 21},
        {'d',  'D',  NULL,         79,  43, 21, 21},
        {'f',  'F',  NULL,         100, 43, 21, 21},
        {'g',  'G',  NULL,         121, 43, 21, 21},
        {'h',  'H',  NULL,         142, 43, 21, 21},
        {'j',  'J',  NULL,         163, 43, 21, 21},
        {'k',  'K',  NULL,         184, 43, 21, 21},
        {'l',  'L',  NULL,         205, 43, 21, 21},
        {';',  ':',  NULL,         226, 43, 21, 21},
        {'\'', '"',  NULL,         247, 43, 21, 21},
        {'\n', '\n', NULL,         268, 43, 48, 21},

        // Row 4
        {'\0', '\0', kbd_shift, 1,   64, 47, 21},
        {'z',  'Z',  NULL,      48,  64, 21, 21},
        {'x',  'X',  NULL,      69,  64, 21, 21},
        {'c',  'C',  NULL,      90,  64, 21, 21},
        {'v',  'V',  NULL,      111, 64, 21, 21},
        {'b',  'B',  NULL,      132, 64, 21, 21},
        {'n',  'N',  NULL,      153, 64, 21, 21},
        {'m',  'M',  NULL,      174, 64, 21, 21},
        {',',  '<',  NULL,      195, 64, 21, 21},
        {'.',  '>',  NULL,      216, 64, 21, 21},
        {'/',  '?',  NULL,      237, 64, 21, 21},
        {'\0', '\0', kbd_shift, 258, 64, 58, 21},

        // Row 5
        {'\0', '\0', kbd_cancel, 1,   85, 94,  21},
        {' ',  ' ',  NULL,       95,  85, 127, 21},
        {'\0', '\0', kbd_finish, 222, 85, 94,  21},
};

static void kbd_update(ui_view* view, void* data, float bx1, float by1, float bx2, float by2) {
    kbd_data* kbdData = (kbd_data*) data;

    if(hidKeysDown() & KEY_A) {
        kbd_finish(view, kbdData);
        return;
    }

    if(hidKeysDown() & KEY_B) {
        kbd_cancel(view, kbdData);
        return;
    }

    u32 kbdTextWidth = 0;
    u32 kbdTextHeight = 0;
    screen_get_texture_size(&kbdTextWidth, &kbdTextHeight, TEXTURE_KBD_TEXT_BG);

    float kbdTextX = bx1 + (bx2 - bx1 - kbdTextWidth) / 2;

    float inputHeight = 0;
    screen_get_string_size_wrap(NULL, &inputHeight, kbdData->input, 0.5f, 0.5f, kbdTextX + kbdTextWidth);

    if(hidKeysHeld() & KEY_TOUCH) {
        touchPosition pos;
        hidTouchRead(&pos);

        u32 kbdWidth = 0;
        u32 kbdHeight = 0;
        screen_get_texture_size(&kbdWidth, &kbdHeight, TEXTURE_KBD_LAYOUT);

        float kbdX = bx1 + (bx2 - bx1 - kbdWidth) / 2;
        float kbdY = by2 - kbdHeight - 2;

        if(pos.px >= kbdX && pos.py >= kbdY && pos.px < kbdX + kbdWidth && pos.py < kbdY + kbdHeight) {
            if((hidKeysDown() & KEY_TOUCH) || (kbdData->keyboardTouched && osGetTime() >= kbdData->nextRepeatTime)) {
                kbdData->keyboardTouched = true;
                kbdData->nextRepeatTime = osGetTime() + ((hidKeysDown() & KEY_TOUCH) ? 500 : 100);

                for(u32 i = 0; i < NUM_KEYS; i++) {
                    LayoutKey key = layout[i];
                    if(pos.px >= kbdX + key.x && pos.py >= kbdY + key.y && pos.px < kbdX + key.x + key.width && pos.py < kbdY + key.y + key.height) {
                        if(key.press != NULL) {
                            key.press(view, kbdData);
                        } else if(kbdData->inputPos < MAX_INPUT_SIZE) {
                            kbdData->input[kbdData->inputPos++] = (kbdData->shift || kbdData->capsLock) ? key.special : key.normal;

                            if(key.normal != key.special) {
                                kbdData->shift = false;
                            }

                            screen_get_string_size_wrap(NULL, &inputHeight, kbdData->input, 0.5f, 0.5f, kbdTextX + kbdTextWidth);

                            if(inputHeight > kbdTextHeight && kbdData->scrollPos < inputHeight - kbdTextHeight) {
                                kbdData->scrollPos = inputHeight - kbdTextHeight;
                            }
                        }

                        break;
                    }
                }
            }
        } else {
            kbdData->keyboardTouched = false;
        }

        if(!(hidKeysDown() & KEY_TOUCH)) {
            float kbdTextX = bx1 + (bx2 - bx1 - kbdTextWidth) / 2;
            float kbdTextY = by1 + 2;

            if(pos.px >= kbdTextX && pos.py >= kbdTextY && pos.px < kbdTextX + kbdTextWidth && pos.py < kbdTextY + kbdTextHeight) {
                kbdData->scrollPos += -((int) pos.py - (int) kbdData->lastTouchY);
            }
        }

        kbdData->lastTouchY = pos.py;
    } else {
        kbdData->keyboardTouched = false;
    }

    if(kbdData->scrollPos > inputHeight - kbdTextHeight) {
        kbdData->scrollPos = inputHeight - kbdTextHeight;
    }

    if(kbdData->scrollPos < 0) {
        kbdData->scrollPos = 0;
    }
}

static void kbd_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    kbd_data* kbdData = (kbd_data*) data;

    if(kbdData->drawTop != NULL) {
        kbdData->drawTop(view, kbdData->data, x1, y1, x2, y2);
    }
}

static void kbd_draw_bottom(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    kbd_data* kbdData = (kbd_data*) data;

    touchPosition pos;
    hidTouchRead(&pos);

    u32 kbdTextWidth = 0;
    u32 kbdTextHeight = 0;
    screen_get_texture_size(&kbdTextWidth, &kbdTextHeight, TEXTURE_KBD_TEXT_BG);

    float kbdTextX = x1 + (x2 - x1 - kbdTextWidth) / 2;
    float kbdTextY = y1 + 2;
    screen_draw_texture(TEXTURE_KBD_TEXT_BG, kbdTextX, kbdTextY, kbdTextWidth, kbdTextHeight);

    screen_set_scissor(true, (u32) (kbdTextX + 1), (u32) (kbdTextY + 1), kbdTextWidth - 1, kbdTextHeight - 1);
    screen_draw_string_wrap(kbdData->input, kbdTextX + 1, kbdTextY + 1 - kbdData->scrollPos, 0.5f, 0.5f, COLOR_TEXT, false, kbdTextX + kbdTextWidth);
    screen_set_scissor(false, 0, 0, 0, 0);

    u32 kbdTextFgWidth = 0;
    u32 kbdTextFgHeight = 0;
    screen_get_texture_size(&kbdTextFgWidth, &kbdTextFgHeight, TEXTURE_KBD_TEXT_FG);

    float kbdTextFgX = kbdTextX + (((int) kbdTextWidth - (int) kbdTextFgWidth) / 2);
    float kbdTextFgY = kbdTextY + (((int) kbdTextHeight - (int) kbdTextFgHeight) / 2);
    screen_draw_texture(TEXTURE_KBD_TEXT_FG, kbdTextFgX, kbdTextFgY, kbdTextFgWidth, kbdTextFgHeight);

    float inputHeight = 0;
    screen_get_string_size_wrap(NULL, &inputHeight, kbdData->input, 0.5f, 0.5f, kbdTextX + kbdTextWidth);

    if(inputHeight > kbdTextHeight) {
        u32 scrollBarWidth = 0;
        screen_get_texture_size(&scrollBarWidth, NULL, TEXTURE_SCROLL_BAR);

        float scrollBarHeight = (kbdTextHeight / inputHeight) * kbdTextHeight;

        float scrollBarX = kbdTextX + kbdTextWidth - scrollBarWidth;
        float scrollBarY = kbdTextY + (kbdData->scrollPos / inputHeight) * kbdTextHeight;

        screen_draw_texture(TEXTURE_SCROLL_BAR, scrollBarX, scrollBarY, scrollBarWidth, scrollBarHeight);
    }

    u32 kbdWidth = 0;
    u32 kbdHeight = 0;
    screen_get_texture_size(&kbdWidth, &kbdHeight, TEXTURE_KBD_LAYOUT);

    float kbdX = x1 + (x2 - x1 - kbdWidth) / 2;
    float kbdY = y2 - kbdHeight - 2;
    screen_draw_texture(TEXTURE_KBD_LAYOUT, kbdX, kbdY, kbdWidth, kbdHeight);

    for(u32 i = 0; i < NUM_KEYS; i++) {
        LayoutKey key = layout[i];

        char text[16];
        if(key.press == kbd_backspace) {
            snprintf(text, sizeof(text), "Del");
        } else if(key.press == kbd_capslock) {
            snprintf(text, sizeof(text), "Caps");
        } else if(key.press == kbd_shift) {
            snprintf(text, sizeof(text), "Shift");
        } else if(key.press == kbd_cancel) {
            snprintf(text, sizeof(text), "Cancel (B)");
        } else if(key.press == kbd_finish) {
            snprintf(text, sizeof(text), "Finish (A)");
        } else if(key.normal == '\t') {
            snprintf(text, sizeof(text), "Tab");
        } else if(key.normal == '\n') {
            snprintf(text, sizeof(text), "Enter");
        } else if(key.normal == ' ') {
            snprintf(text, sizeof(text), "Space");
        } else {
            snprintf(text, sizeof(text), "%c", (kbdData->shift || kbdData->capsLock) ? key.special : key.normal);
        }

        float textWidth = 0;
        float textHeight = 0;
        screen_get_string_size(&textWidth, &textHeight, text, 0.5f, 0.5f);

        float textX = kbdX + key.x + (key.width - textWidth) / 2;
        float textY = kbdY + key.y + (key.height - textHeight) / 2;
        screen_draw_string(text, textX, textY, 0.5f, 0.5f, COLOR_TEXT, true);

        if((key.press == kbd_capslock && kbdData->capsLock)
           || (key.press == kbd_shift && kbdData->shift)
           || ((hidKeysHeld() & KEY_TOUCH) && pos.px >= kbdX + key.x && pos.py >= kbdY + key.y && pos.px < kbdX + key.x + key.width && pos.py < kbdY + key.y + key.height)) {
            screen_draw_texture(TEXTURE_KBD_PRESS_OVERLAY, kbdX + key.x, kbdY + key.y, key.width, key.height);
        }
    }
}

void kbd_display(const char* name, void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2),
                                               void (*finished)(void* data, char* input),
                                               void (*canceled)(void* data)) {
    kbd_data* kbdData = (kbd_data*) calloc(1, sizeof(kbd_data));
    if(kbdData == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate info data.");
        return;
    }

    memset(kbdData->input, '\0', MAX_INPUT_SIZE);
    kbdData->inputPos = 0;

    kbdData->shift = false;
    kbdData->capsLock = false;

    kbdData->scrollPos = 0;
    kbdData->lastTouchY = 0;

    kbdData->keyboardTouched = false;
    kbdData->nextRepeatTime = 0;

    kbdData->data = data;
    kbdData->drawTop = drawTop;
    kbdData->finished = finished;
    kbdData->canceled = canceled;

    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    if(view == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate UI view.");
        return;
    }

    view->name = name;
    view->info = "";
    view->data = kbdData;
    view->update = kbd_update;
    view->drawTop = kbd_draw_top;
    view->drawBottom = kbd_draw_bottom;
    ui_push(view);
}