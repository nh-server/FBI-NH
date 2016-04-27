#include <malloc.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"

typedef struct {
    linked_list* items;
    list_item* selected;
} delete_ext_save_data_data;

static void action_delete_ext_save_data_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_ext_save_data_info(view, ((delete_ext_save_data_data*) data)->selected->data, x1, y1, x2, y2);
}

static void action_delete_ext_save_data_update(ui_view* view, void* data, float* progress, char* text) {
    delete_ext_save_data_data* deleteData = (delete_ext_save_data_data*) data;
    ext_save_data_info* info = (ext_save_data_info*) deleteData->selected->data;

    FS_ExtSaveDataInfo extInfo = {.mediaType = info->mediaType, .saveId = info->extSaveDataId};
    Result res = FSUSER_DeleteExtSaveData(extInfo);

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(NULL, info, ui_draw_ext_save_data_info, res, "Failed to delete ext save data.");
    } else {
        linked_list_remove(deleteData->items, deleteData->selected);
        task_free_ext_save_data(deleteData->selected);

        prompt_display("Success", "Ext save data deleted.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
    }

    free(data);
}

static void action_delete_ext_save_data_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        info_display("Deleting Ext Save Data", "", false, data, action_delete_ext_save_data_update, action_delete_ext_save_data_draw_top);
    } else {
        free(data);
    }
}

void action_delete_ext_save_data(linked_list* items, list_item* selected) {
    delete_ext_save_data_data* data = (delete_ext_save_data_data*) calloc(1, sizeof(delete_ext_save_data_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate delete ext save data data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    prompt_display("Confirmation", "Delete the selected ext save data?", COLOR_TEXT, true, data, NULL, action_delete_ext_save_data_draw_top, action_delete_ext_save_data_onresponse);
}