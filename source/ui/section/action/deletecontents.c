#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../screen.h"
#include "../../../util.h"
#include "../task/task.h"

typedef struct {
    file_info* base;
    bool* populated;
    char** contents;

    data_op_info deleteInfo;
    Handle cancelEvent;
} delete_contents_data;

static Result action_delete_contents_delete(void* data, u32 index) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    FS_Path* fsPath = util_make_path_utf8(deleteData->contents[index]);

    Result res = 0;
    if(util_is_dir(deleteData->base->archive, deleteData->contents[index])) {
        res = FSUSER_DeleteDirectory(*deleteData->base->archive, *fsPath);
    } else {
        res = FSUSER_DeleteFile(*deleteData->base->archive, *fsPath);
    }

    util_free_path_utf8(fsPath);

    return res;
}

static bool action_delete_contents_result_error(void* data, u32 index, Result res) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(res == MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED)) {
        ui_push(prompt_create("Failure", "Delete cancelled.", COLOR_TEXT, false, deleteData->base, NULL, ui_draw_file_info, NULL));
        return false;
    } else {
        char* path = deleteData->contents[index];

        volatile bool dismissed = false;
        if(strlen(path) > 48) {
            error_display_res(&dismissed, deleteData->base, ui_draw_file_info, res, "Failed to delete content.\n%.45s...", path);
        } else {
            error_display_res(&dismissed, deleteData->base, ui_draw_file_info, res, "Failed to delete content.\n%.48s", path);
        }

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < deleteData->deleteInfo.total - 1;
}

static bool action_delete_contents_io_error(void* data, u32 index, int err) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    char* path = deleteData->contents[index];

    volatile bool dismissed = false;
    if(strlen(path) > 48) {
        error_display_errno(&dismissed, deleteData->base, ui_draw_file_info, err, "Failed to delete content.\n%.45s...", path);
    } else {
        error_display_errno(&dismissed, deleteData->base, ui_draw_file_info, err, "Failed to delete content.\n%.48s", path);
    }

    while(!dismissed) {
        svcSleepThread(1000000);
    }

    return index < deleteData->deleteInfo.total - 1;
}

static void action_delete_contents_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((delete_contents_data*) data)->base, x1, y1, x2, y2);
}

static void action_delete_contents_free_data(delete_contents_data* data) {
    util_free_contents(data->contents, data->deleteInfo.total);
    free(data);
}

static void action_delete_contents_done_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    action_delete_contents_free_data((delete_contents_data*) data);
}

static void action_delete_contents_update(ui_view* view, void* data, float* progress, char* progressText) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(deleteData->deleteInfo.finished) {
        *deleteData->populated = false;

        if(deleteData->base->archive->id == ARCHIVE_USER_SAVEDATA) {
            FSUSER_ControlArchive(*deleteData->base->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
        }

        progressbar_destroy(view);
        ui_pop();

        if(deleteData->deleteInfo.premature) {
            action_delete_contents_free_data(deleteData);
        } else {
            ui_push(prompt_create("Success", "Contents deleted.", COLOR_TEXT, false, data, NULL, action_delete_contents_draw_top, action_delete_contents_done_onresponse));
        }

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(deleteData->cancelEvent);
    }

    *progress = deleteData->deleteInfo.total > 0 ? (float) deleteData->deleteInfo.processed / (float) deleteData->deleteInfo.total : 0;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu", deleteData->deleteInfo.processed, deleteData->deleteInfo.total);
}

static void action_delete_contents_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(response) {
        deleteData->cancelEvent = task_data_op(&deleteData->deleteInfo);
        if(deleteData->cancelEvent != 0) {
            ui_view* progressView = progressbar_create("Deleting Contents", "Press B to cancel.", data, action_delete_contents_update, action_delete_contents_draw_top);
            snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", deleteData->deleteInfo.total);
            ui_push(progressView);
        } else {
            error_display(NULL, NULL, NULL, "Failed to initiate delete operation.");
        }
    } else {
        action_delete_contents_free_data(deleteData);
    }
}

static void action_delete_contents_internal(file_info* info, bool* populated, const char* message, bool recursive, void* filterData, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    delete_contents_data* data = (delete_contents_data*) calloc(1, sizeof(delete_contents_data));
    data->base = info;
    data->populated = populated;

    data->deleteInfo.data = data;

    data->deleteInfo.op = DATAOP_DELETE;

    data->deleteInfo.delete = action_delete_contents_delete;

    data->deleteInfo.resultError = action_delete_contents_result_error;
    data->deleteInfo.ioError = action_delete_contents_io_error;

    data->cancelEvent = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->deleteInfo.total, info->archive, info->path, recursive, false, filterData, filter))) {
        error_display_res(NULL, info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", message, COLOR_TEXT, true, data, NULL, action_delete_contents_draw_top, action_delete_contents_onresponse));
}

void action_delete_contents(file_info* info, bool* populated) {
    action_delete_contents_internal(info, populated, "Delete the selected content?", true, NULL, NULL);
}

void action_delete_dir(file_info* info, bool* populated) {
    action_delete_contents_internal(info, populated, "Delete the current directory?", true, NULL, NULL);
}

void action_delete_dir_contents(file_info* info, bool* populated) {
    action_delete_contents_internal(info, populated, "Delete all contents of the current directory?", true, info->path, util_filter_not_path);
}

void action_delete_dir_cias(file_info* info, bool* populated) {
    action_delete_contents_internal(info, populated, "Delete all CIAs in the current directory?", false, ".cia", util_filter_file_extension);
}