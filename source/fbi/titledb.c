#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "resources.h"
#include "section.h"
#include "action/action.h"
#include "task/uitask.h"
#include "../core/core.h"

static list_item install = {"Install", COLOR_TEXT, action_install_titledb};
static list_item mark_updated = {"Mark Updated", COLOR_TEXT, action_mark_titledb_updated};

typedef struct {
    populate_titledb_data populateData;

    bool showCIAs;
    bool show3DSXs;
    bool sortByName;
    bool sortByUpdate;
    bool sortByStatus;

    bool populated;
} titledb_data;

typedef struct {
    titledb_data* parent;
    linked_list* items;
} titledb_options_data;

typedef struct {
    linked_list* items;
    list_item* selected;
} titledb_entry_data;

typedef struct {
    linked_list* items;
    list_item* selected;
    bool cia;
} titledb_action_data;

static void titledb_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    titledb_action_data* actionData = (titledb_action_data*) data;

    if(actionData->cia) {
        task_draw_titledb_info_cia(view, actionData->selected->data, x1, y1, x2, y2);
    } else {
        task_draw_titledb_info_tdsx(view, actionData->selected->data, x1, y1, x2, y2);
    }
}

static void titledb_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_action_data* actionData = (titledb_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(linked_list*, list_item*, bool) = (void(*)(linked_list*, list_item*, bool)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->items, actionData->selected, actionData->cia);

        free(data);

        return;
    }

    if(linked_list_size(items) == 0) {
        linked_list_add(items, &install);
        linked_list_add(items, &mark_updated);
    }
}

static void titledb_action_open(linked_list* items, list_item* selected, bool cia) {
    titledb_action_data* data = (titledb_action_data*) calloc(1, sizeof(titledb_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB action data.");

        return;
    }

    data->items = items;
    data->selected = selected;
    data->cia = cia;

    list_display("TitleDB Action", "A: Select, B: Return", data, titledb_action_update, titledb_action_draw_top);
}

static void titledb_entry_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    titledb_entry_data* entryData = (titledb_entry_data*) data;

    if(selected != NULL) {
        if(strncmp(selected->name, "CIA", sizeof(selected->name)) == 0) {
            task_draw_titledb_info_cia(view, entryData->selected->data, x1, y1, x2, y2);
        } else if(strncmp(selected->name, "3DSX", sizeof(selected->name)) == 0) {
            task_draw_titledb_info_tdsx(view, entryData->selected->data, x1, y1, x2, y2);
        }
    }
}

static void titledb_entry_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_entry_data* entryData = (titledb_entry_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();

        linked_list_iter iter;
        linked_list_iterate(items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            free(linked_list_iter_next(&iter));
            linked_list_iter_remove(&iter);
        }

        list_destroy(view);
        free(data);

        return;
    }

    if(selected != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        titledb_action_open(entryData->items, entryData->selected, (bool) selected->data);
        return;
    }

    titledb_info* info = (titledb_info*) entryData->selected->data;
    if(linked_list_size(items) == 0) {
        if(info->cia.exists) {
            list_item* item = (list_item*) calloc(1, sizeof(list_item));
            if(item != NULL) {
                string_copy(item->name, "CIA", sizeof(item->name));
                item->data = (void*) true;
                item->color = info->cia.installed ? info->cia.installedInfo.id != info->cia.id ? COLOR_TITLEDB_OUTDATED : COLOR_TITLEDB_INSTALLED : COLOR_TITLEDB_NOT_INSTALLED;

                linked_list_add(items, item);
            }
        }

        if(info->tdsx.exists) {
            list_item* item = (list_item*) calloc(1, sizeof(list_item));
            if(item != NULL) {
                string_copy(item->name, "3DSX", sizeof(item->name));
                item->data = (void*) false;
                item->color = info->tdsx.installed ? info->tdsx.installedInfo.id != info->tdsx.id ? COLOR_TITLEDB_OUTDATED : COLOR_TITLEDB_INSTALLED : COLOR_TITLEDB_NOT_INSTALLED;

                linked_list_add(items, item);
            }
        }
    } else {
        linked_list_iter iter;
        linked_list_iterate(items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            list_item* item = (list_item*) linked_list_iter_next(&iter);

            if((bool) item->data) {
                item->color = info->cia.installed ? info->cia.installedInfo.id != info->cia.id ? COLOR_TITLEDB_OUTDATED : COLOR_TITLEDB_INSTALLED : COLOR_TITLEDB_NOT_INSTALLED;
            } else {
                item->color = info->tdsx.installed ? info->tdsx.installedInfo.id != info->tdsx.id ? COLOR_TITLEDB_OUTDATED : COLOR_TITLEDB_INSTALLED : COLOR_TITLEDB_NOT_INSTALLED;
            }
        }
    }
}

static void titledb_entry_open(linked_list* items, list_item* selected) {
    titledb_entry_data* data = (titledb_entry_data*) calloc(1, sizeof(titledb_entry_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB entry data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    list_display("TitleDB Entry", "A: Select, B: Return", data, titledb_entry_update, titledb_entry_draw_top);
}

static int titledb_compare(void* data, const void* p1, const void* p2) {
    titledb_data* listData = (titledb_data*) data;

    list_item* info1 = (list_item*) p1;
    list_item* info2 = (list_item*) p2;

    titledb_info* data1 = (titledb_info*) info1->data;
    titledb_info* data2 = (titledb_info*) info2->data;

    if(listData->sortByName) {
        return strncasecmp(info1->name, info2->name, sizeof(info1->name));
    } else if(listData->sortByUpdate) {
        return strncasecmp(data2->mtime, data1->mtime, sizeof(data2->mtime));
    } else if(listData->sortByStatus) {
        bool outdated1 = (data1->cia.installed && data1->cia.installedInfo.id != data1->cia.id)
                         || (data1->tdsx.installed && data1->tdsx.installedInfo.id != data1->tdsx.id);
        bool outdated2 = (data2->cia.installed && data2->cia.installedInfo.id != data2->cia.id)
                         || (data2->tdsx.installed && data2->tdsx.installedInfo.id != data2->tdsx.id);

        if(outdated1 && !outdated2) {
            return -1;
        } else if(!outdated1 && outdated2) {
            return 1;
        } else {
            bool installed1 = data1->cia.installed || data1->tdsx.installed;
            bool installed2 = data2->cia.installed || data2->tdsx.installed;

            if(installed1 && !installed2) {
                return -1;
            } else if(!installed1 && installed2) {
                return 1;
            } else {
                return strncasecmp(info1->name, info2->name, sizeof(info1->name));
            }
        }
    } else {
        return 0;
    }
}

static void titledb_options_add_entry(linked_list* items, const char* name, bool* val) {
    list_item* item = (list_item*) calloc(1, sizeof(list_item));
    if(item != NULL) {
        snprintf(item->name, LIST_ITEM_NAME_MAX, "%s", name);
        item->color = *val ? COLOR_ENABLED : COLOR_DISABLED;
        item->data = val;

        linked_list_add(items, item);
    }
}

static void titledb_options_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_options_data* optionsData = (titledb_options_data*) data;
    titledb_data* listData = optionsData->parent;

    if(hidKeysDown() & KEY_B) {
        linked_list_iter iter;
        linked_list_iterate(items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            free(linked_list_iter_next(&iter));
            linked_list_iter_remove(&iter);
        }

        ui_pop();
        list_destroy(view);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        bool* val = (bool*) selected->data;
        *val = !(*val);

        if(val == &listData->sortByName || val == &listData->sortByUpdate || val == &listData->sortByStatus) {
            if(*val) {
                if(val == &listData->sortByName) {
                    listData->sortByUpdate = false;
                    listData->sortByStatus = false;
                } else if(val == &listData->sortByUpdate) {
                    listData->sortByName = false;
                    listData->sortByStatus = false;
                } else if(val == &listData->sortByStatus) {
                    listData->sortByName = false;
                    listData->sortByUpdate = false;
                }

                linked_list_iter iter;
                linked_list_iterate(items, &iter);
                while(linked_list_iter_has_next(&iter)) {
                    list_item* item = (list_item*) linked_list_iter_next(&iter);

                    item->color = *(bool*) item->data ? COLOR_ENABLED : COLOR_DISABLED;
                }
            } else {
                selected->color = *val ? COLOR_ENABLED : COLOR_DISABLED;
            }

            linked_list_sort(optionsData->items, listData, titledb_compare);
        } else {
            selected->color = *val ? COLOR_ENABLED : COLOR_DISABLED;

            listData->populated = false;
        }
    }

    if(linked_list_size(items) == 0) {
        titledb_options_add_entry(items, "Show CIAs", &listData->showCIAs);
        titledb_options_add_entry(items, "Show 3DSXs", &listData->show3DSXs);
        titledb_options_add_entry(items, "Sort by name", &listData->sortByName);
        titledb_options_add_entry(items, "Sort by update date", &listData->sortByUpdate);
        titledb_options_add_entry(items, "Sort by install status", &listData->sortByStatus);
    }
}

static void titledb_options_open(titledb_data* parent, linked_list* items) {
    titledb_options_data* data = (titledb_options_data*) calloc(1, sizeof(titledb_options_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB options data.");

        return;
    }

    data->parent = parent;
    data->items = items;

    list_display("Options", "A: Toggle, B: Return", data, titledb_options_update, NULL);
}

static void titledb_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    titledb_data* listData = (titledb_data*) data;

    if(!listData->populateData.itemsListed) {
        static const char* text = "Loading title list, please wait...\nNOTE: Cancelling may take up to 15 seconds.";

        float textWidth;
        float textHeight;
        screen_get_string_size(&textWidth, &textHeight, text, 0.5f, 0.5f);
        screen_draw_string(text, x1 + (x2 - x1 - textWidth) / 2, y1 + (y2 - y1 - textHeight) / 2, 0.5f, 0.5f, COLOR_TEXT, true);
    } else if(selected != NULL && selected->data != NULL) {
        task_draw_titledb_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void titledb_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_data* listData = (titledb_data*) data;

    svcSignalEvent(listData->populateData.resumeEvent);

    if(hidKeysDown() & KEY_B) {
        if(!listData->populateData.finished) {
            svcSignalEvent(listData->populateData.cancelEvent);
            while(!listData->populateData.finished) {
                svcSleepThread(1000000);
            }
        }

        ui_pop();

        task_clear_titledb(items);
        list_destroy(view);

        free(listData);
        return;
    }

    if(!listData->populated || (hidKeysDown() & KEY_X)) {
        if(!listData->populateData.finished) {
            svcSignalEvent(listData->populateData.cancelEvent);
            while(!listData->populateData.finished) {
                svcSleepThread(1000000);
            }
        }

        listData->populateData.items = items;
        Result res = task_populate_titledb(&listData->populateData);
        if(R_FAILED(res)) {
            error_display_res(NULL, NULL, res, "Failed to initiate TitleDB list population.");
        }

        listData->populated = true;
    }

    if(listData->populateData.itemsListed) {
        if(hidKeysDown() & KEY_Y) {
            action_update_titledb(items, selected);
            return;
        }

        if(hidKeysDown() & KEY_SELECT) {
            titledb_options_open(listData, items);
            return;
        }
    }

    if(listData->populateData.finished && R_FAILED(listData->populateData.result)) {
        error_display_res(NULL, NULL, listData->populateData.result, "Failed to populate TitleDB list.");

        listData->populateData.result = 0;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        svcClearEvent(listData->populateData.resumeEvent);

        titledb_entry_open(items, selected);
        return;
    }
}

static bool titledb_filter(void* data, titledb_info* info) {
    titledb_data* listData = (titledb_data*) data;

    return (info->cia.exists && listData->showCIAs) || (info->tdsx.exists && listData->show3DSXs);
}

void titledb_open() {
    titledb_data* data = (titledb_data*) calloc(1, sizeof(titledb_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB data.");

        return;
    }

    data->showCIAs = true;
    data->show3DSXs = true;
    data->sortByName = true;
    data->sortByUpdate = false;
    data->sortByStatus = false;

    data->populateData.finished = true;

    data->populateData.userData = data;
    data->populateData.filter = titledb_filter;
    data->populateData.compare = titledb_compare;

    list_display("TitleDB.com", "A: Select, B: Return, X: Refresh, Y: Update All, Select: Options", data, titledb_update, titledb_draw_top);
}