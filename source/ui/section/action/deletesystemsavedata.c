#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"

static void action_delete_system_save_data_success_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_delete_system_save_data_update(ui_view* view, void* data, float* progress, char* progressText) {
    system_save_data_info* info = (system_save_data_info*) data;

    FS_SystemSaveDataInfo sysInfo = *(FS_SystemSaveDataInfo*) &info->systemSaveDataId;
    Result res = FSUSER_DeleteSystemSaveData(sysInfo);
    if(R_FAILED(res)) {
        progressbar_destroy(view);
        ui_pop();

        error_display_res(info, ui_draw_system_save_data_info, res, "Failed to delete system save data.");

        return;
    }

    progressbar_destroy(view);
    ui_pop();

    ui_push(prompt_create("Success", "System save data deleted.", 0xFF000000, false, info, NULL, ui_draw_system_save_data_info, action_delete_system_save_data_success_onresponse));
}

static void action_delete_system_save_data_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_push(progressbar_create("Deleting System Save Data", "", data, action_delete_system_save_data_update, ui_draw_system_save_data_info));
    }
}

void action_delete_system_save_data(system_save_data_info* info) {
    ui_push(prompt_create("Confirmation", "Delete the selected system save data?", 0xFF000000, true, info, NULL, ui_draw_system_save_data_info, action_delete_system_save_data_onresponse));
}