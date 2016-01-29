#include <3ds.h>
#include <malloc.h>

#include "list.h"
#include "section/task.h"
#include "../screen.h"

typedef struct {
    void* data;
    list_item* items;
    u32* itemCount;
    u32 selectedIndex;
    u32 selectionScroll;
    u64 nextSelectionScrollResetTime;
    float scrollPos;
    u32 lastScrollTouchY;
    u64 nextActionTime;
    void (*update)(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched);
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected);
} list_data;

static float list_get_item_screen_y(list_data* listData, u32 index) {
    float y = -listData->scrollPos;
    for(u32 i = 0; i < index && i < *listData->itemCount; i++) {
        float stringHeight;
        screen_get_string_size(NULL, &stringHeight, listData->items[i].name, 0.5f, 0.5f);
        y += stringHeight;
    }

    return y;
}

static int list_get_item_at(list_data* listData, float screenY) {
    float y = -listData->scrollPos;
    for(u32 i = 0; i < *listData->itemCount; i++) {
        float stringHeight;
        screen_get_string_size(NULL, &stringHeight, listData->items[i].name, 0.5f, 0.5f);

        if(screenY >= y && screenY < y + stringHeight) {
            return (int) i;
        }

        y += stringHeight;
    }

    return -1;
}

static void list_validate_pos(list_data* listData, float by1, float by2) {
    if(listData->items == NULL || listData->itemCount == NULL || *listData->itemCount <= 0 || listData->selectedIndex <= 0) {
        listData->selectedIndex = 0;
        listData->scrollPos = 0;
    }

    if(listData->items != NULL && listData->itemCount != NULL && *listData->itemCount > 0) {
        if(listData->selectedIndex > *listData->itemCount - 1) {
            listData->selectedIndex = *listData->itemCount - 1;
            listData->scrollPos = 0;
        }

        float lastItemHeight;
        screen_get_string_size(NULL, &lastItemHeight, listData->items[*listData->itemCount - 1].name, 0.5f, 0.5f);

        float lastPageEnd = list_get_item_screen_y(listData, *listData->itemCount - 1);
        if(lastPageEnd < by2 - by1 - lastItemHeight) {
            listData->scrollPos -= (by2 - by1 - lastItemHeight) - lastPageEnd;
        }

        if(listData->scrollPos < 0) {
            listData->scrollPos = 0;
        }
    }
}

static void list_update(ui_view* view, void* data, float bx1, float by1, float bx2, float by2) {
    svcWaitSynchronization(task_get_mutex(), U64_MAX);

    list_data* listData = (list_data*) data;

    bool selectedTouched = false;
    if(listData->items != NULL && listData->itemCount != NULL && *listData->itemCount > 0) {
        list_validate_pos(listData, by1, by2);

        float itemWidth;
        screen_get_string_size(&itemWidth, NULL, listData->items[listData->selectedIndex].name, 0.5f, 0.5f);
        if(itemWidth > bx2 - bx1) {
            if(listData->selectionScroll == 0 || listData->selectionScroll >= itemWidth - (bx2 - bx1)) {
                if(listData->nextSelectionScrollResetTime == 0) {
                    listData->nextSelectionScrollResetTime = osGetTime() + 2000;
                } else if(osGetTime() >= listData->nextSelectionScrollResetTime) {
                    listData->selectionScroll = listData->selectionScroll == 0 ? 1 : 0;
                    listData->nextSelectionScrollResetTime = 0;
                }
            } else {
                listData->selectionScroll++;
            }
        } else {
            listData->selectionScroll = 0;
            listData->nextSelectionScrollResetTime = 0;
        }

        u32 lastSelectedIndex = listData->selectedIndex;

        if(((hidKeysDown() & KEY_DOWN) || ((hidKeysHeld() & KEY_DOWN) && osGetTime() >= listData->nextActionTime)) && listData->selectedIndex < *listData->itemCount - 1) {
            listData->selectedIndex++;
            listData->nextActionTime = osGetTime() + ((hidKeysDown() & KEY_DOWN) ? 500 : 100);
        }

        if(((hidKeysDown() & KEY_UP) || ((hidKeysHeld() & KEY_UP) && osGetTime() >= listData->nextActionTime)) && listData->selectedIndex > 0) {
            listData->selectedIndex--;
            listData->nextActionTime = osGetTime() + ((hidKeysDown() & KEY_UP) ? 500 : 100);
        }

        if(((hidKeysDown() & KEY_RIGHT) || ((hidKeysHeld() & KEY_RIGHT) && osGetTime() >= listData->nextActionTime)) && listData->selectedIndex < *listData->itemCount - 1) {
            u32 remaining = *listData->itemCount - 1 - listData->selectedIndex;

            listData->selectedIndex += remaining < 13 ? remaining : 13;
            listData->nextActionTime = osGetTime() + ((hidKeysDown() & KEY_RIGHT) ? 500 : 100);
        }

        if(((hidKeysDown() & KEY_LEFT) || ((hidKeysHeld() & KEY_LEFT) && osGetTime() >= listData->nextActionTime)) && listData->selectedIndex > 0) {
            u32 remaining = listData->selectedIndex;

            listData->selectedIndex -= remaining < 13 ? remaining : 13;
            listData->nextActionTime = osGetTime() + ((hidKeysDown() & KEY_LEFT) ? 500 : 100);
        }

        if(lastSelectedIndex != listData->selectedIndex) {
            listData->selectionScroll = 0;
            listData->nextSelectionScrollResetTime = 0;

            float itemHeight;
            screen_get_string_size(NULL, &itemHeight, listData->items[listData->selectedIndex].name, 0.5f, 0.5f);

            float itemY = list_get_item_screen_y(listData, listData->selectedIndex);
            if(itemY + itemHeight > by2 - by1) {
                listData->scrollPos -= (by2 - by1) - itemY - itemHeight;
            }

            if(itemY < 0) {
                listData->scrollPos += itemY;
            }
        }

        if(hidKeysDown() & KEY_TOUCH) {
            touchPosition pos;
            hidTouchRead(&pos);

            listData->lastScrollTouchY = pos.py;

            int index = list_get_item_at(listData, pos.py - by1);
            if(index >= 0) {
                if(listData->selectedIndex == index) {
                    selectedTouched = true;
                } else {
                    listData->selectedIndex = (u32) index;
                }
            }
        } else if(hidKeysHeld() & KEY_TOUCH) {
            touchPosition pos;
            hidTouchRead(&pos);

            listData->scrollPos += -((int) pos.py - (int) listData->lastScrollTouchY);
            listData->lastScrollTouchY = pos.py;
        }

        list_validate_pos(listData, by1, by2);
    }

    if(listData->update != NULL) {
        listData->update(view, listData->data, &listData->items, &listData->itemCount, listData->items != NULL && listData->itemCount != NULL && *listData->itemCount > 0 ? &listData->items[listData->selectedIndex] : NULL, selectedTouched);
    }

    svcReleaseMutex(task_get_mutex());
}

static void list_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    svcWaitSynchronization(task_get_mutex(), U64_MAX);

    list_data* listData = (list_data*) data;

    if(listData->drawTop != NULL) {
        listData->drawTop(view, listData->data, x1, y1, x2, y2, listData->items != NULL && listData->itemCount != NULL && *listData->itemCount > 0 ? &listData->items[listData->selectedIndex] : NULL);
    }

    svcReleaseMutex(task_get_mutex());
}

static void list_draw_bottom(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    svcWaitSynchronization(task_get_mutex(), U64_MAX);

    list_data* listData = (list_data*) data;

    list_validate_pos(listData, y1, y2);

    if(listData->items != NULL && listData->itemCount != NULL) {
        float y = y1 - listData->scrollPos;
        for(u32 i = 0; i < *listData->itemCount && y < y2; i++) {
            float stringHeight;
            screen_get_string_size(NULL, &stringHeight, listData->items[i].name, 0.5f, 0.5f);

            if(y > y1 - stringHeight) {
                float x = x1 + 2;
                if(i == listData->selectedIndex) {
                    x -= listData->selectionScroll;
                }

                screen_draw_string(listData->items[i].name, x, y, 0.5f, 0.5f, listData->items[i].rgba, false);

                if(i == listData->selectedIndex) {
                    u32 selectionOverlayWidth = 0;
                    u32 selectionOverlayHeight = 0;
                    screen_get_texture_size(&selectionOverlayWidth, &selectionOverlayHeight, TEXTURE_SELECTION_OVERLAY);
                    screen_draw_texture(TEXTURE_SELECTION_OVERLAY, (x1 + x2 - selectionOverlayWidth) / 2, y, selectionOverlayWidth, stringHeight);
                }
            }

            y += stringHeight;
        }
    }

    svcReleaseMutex(task_get_mutex());
}

ui_view* list_create(const char* name, const char* info, void* data, void (*update)(ui_view* view, void* data, list_item** contents, u32** itemCount, list_item* selected, bool selectedTouched),
                                                                     void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected)) {
    list_data* listData = (list_data*) calloc(1, sizeof(list_data));
    listData->data = data;
    listData->items = NULL;
    listData->itemCount = NULL;
    listData->selectedIndex = 0;
    listData->selectionScroll = 0;
    listData->nextSelectionScrollResetTime = 0;
    listData->scrollPos = 0;
    listData->lastScrollTouchY = 0;
    listData->update = update;
    listData->drawTop = drawTop;

    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    view->name = name;
    view->info = info;
    view->data = listData;
    view->update = list_update;
    view->drawTop = list_draw_top;
    view->drawBottom = list_draw_bottom;
    return view;
}

void list_destroy(ui_view* view) {
    free(view->data);
    free(view);
}

void* list_get_data(ui_view* view) {
    return ((list_data*) view->data)->data;
}