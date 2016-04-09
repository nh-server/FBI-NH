#include <malloc.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../task/task.h"

typedef struct {
    ext_save_data_info* info;
    bool* populated;
} delete_ext_save_data_data;

static void action_delete_ext_save_data_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_ext_save_data_info(view, ((delete_ext_save_data_data*) data)->info, x1, y1, x2, y2);
}

static void action_delete_ext_save_data_success_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_delete_ext_save_data_update(ui_view* view, void* data, float* progress, char* progressText) {
    delete_ext_save_data_data* deleteData = (delete_ext_save_data_data*) data;

    FS_ExtSaveDataInfo extInfo = {.mediaType = deleteData->info->mediaType, .saveId = deleteData->info->extSaveDataId};
    Result res = FSUSER_DeleteExtSaveData(extInfo);

    progressbar_destroy(view);
    ui_pop();

    if(R_FAILED(res)) {
        error_display_res(deleteData->info, ui_draw_ext_save_data_info, res, "Failed to delete ext save data.");
    } else {
        *deleteData->populated = false;

        ui_push(prompt_create("Success", "Ext save data deleted.", 0xFF000000, false, deleteData->info, NULL, ui_draw_ext_save_data_info, action_delete_ext_save_data_success_onresponse));
    }

    free(data);
}

static void action_delete_ext_save_data_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_push(progressbar_create("Deleting Ext Save Data", "", data, action_delete_ext_save_data_update, action_delete_ext_save_data_draw_top));
    } else {
        free(data);
    }
}

void action_delete_ext_save_data(ext_save_data_info* info, bool* populated) {
    delete_ext_save_data_data* data = (delete_ext_save_data_data*) calloc(1, sizeof(delete_ext_save_data_data));
    data->info = info;
    data->populated = populated;

    ui_push(prompt_create("Confirmation", "Delete the selected ext save data?", 0xFF000000, true, data, NULL, action_delete_ext_save_data_draw_top, action_delete_ext_save_data_onresponse));
}