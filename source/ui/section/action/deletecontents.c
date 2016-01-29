#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../util.h"

typedef struct {
    file_info* base;
    u32 processed;
    u32 total;
    char** contents;
} delete_dir_contents_data;

static void action_delete_dir_contents_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((delete_dir_contents_data*) data)->base, x1, y1, x2, y2);
}

static void action_delete_dir_contents_free_data(delete_dir_contents_data* data) {
    util_free_contents(data->contents, data->total);
    free(data);
}

static void action_delete_dir_contents_done_onresponse(ui_view* view, void* data, bool response) {
    action_delete_dir_contents_free_data((delete_dir_contents_data*) data);

    if(task_get_files_path() != NULL && !util_is_dir(task_get_files_archive(), task_get_files_path())) {
        char parentPath[PATH_MAX];
        util_get_parent_path(parentPath, task_get_files_path(), PATH_MAX);

        strncpy(task_get_files_path(), parentPath, PATH_MAX);
    }

    task_refresh_files();

    prompt_destroy(view);
}

static void action_delete_dir_contents_update(ui_view* view, void* data, float* progress, char* progressText) {
    delete_dir_contents_data* deleteData = (delete_dir_contents_data*) data;

    if(hidKeysDown() & KEY_B) {
        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Failure", "Delete cancelled.", 0xFF000000, false, data, NULL, action_delete_dir_contents_draw_top, action_delete_dir_contents_done_onresponse));
        return;
    }

    if(deleteData->processed >= deleteData->total) {
        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Success", "Contents deleted.", 0xFF000000, false, data, NULL, action_delete_dir_contents_draw_top, action_delete_dir_contents_done_onresponse));
    } else {
        FS_Archive* archive = deleteData->base->archive;
        char* path = deleteData->contents[deleteData->processed];
        FS_Path fsPath = fsMakePath(PATH_ASCII, path);

        Result res = 0;
        if(util_is_dir(archive, path)) {
            res = FSUSER_DeleteDirectory(*archive, fsPath);
        } else {
            res = FSUSER_DeleteFile(*archive, fsPath);
        }

        if(R_FAILED(res)) {
            if(deleteData->processed >= deleteData->total - 1) {
                ui_pop();
            }

            if(strlen(path) > 48) {
                error_display_res(deleteData->base, ui_draw_file_info, res, "Failed to delete content.\n%.45s...", path);
            } else {
                error_display_res(deleteData->base, ui_draw_file_info, res, "Failed to delete content.\n%.48s", path);
            }

            if(deleteData->processed >= deleteData->total - 1) {
                action_delete_dir_contents_free_data(deleteData);
                progressbar_destroy(view);
                return;
            }
        }

        deleteData->processed++;

        *progress = (float) deleteData->processed / (float) deleteData->total;
        snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu", deleteData->processed, deleteData->total);
    }
}

static void action_delete_dir_contents_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_view* progressView = progressbar_create("Deleting Contents", "Press B to cancel.", data, action_delete_dir_contents_update, action_delete_dir_contents_draw_top);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", ((delete_dir_contents_data*) data)->total);
        ui_push(progressView);
    }
}

void action_delete_contents(file_info* info) {
    delete_dir_contents_data* data = (delete_dir_contents_data*) calloc(1, sizeof(delete_dir_contents_data));
    data->base = info;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, info->archive, info->path, true, false, NULL, NULL))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Delete the selected content?", 0xFF000000, true, data, NULL, action_delete_dir_contents_draw_top, action_delete_dir_contents_onresponse));
}

void action_delete_dir_contents(file_info* info) {
    delete_dir_contents_data* data = (delete_dir_contents_data*) calloc(1, sizeof(delete_dir_contents_data));
    data->base = info;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, info->archive, info->path, true, false, info->path, util_filter_not_path))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Delete all contents of the selected directory?", 0xFF000000, true, data, NULL, action_delete_dir_contents_draw_top, action_delete_dir_contents_onresponse));
}

void action_delete_dir_cias(file_info* info) {
    delete_dir_contents_data* data = (delete_dir_contents_data*) calloc(1, sizeof(delete_dir_contents_data));
    data->base = info;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, info->archive, info->path, false, false, ".cia", util_filter_file_extension))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Delete all CIAs in the selected directory?", 0xFF000000, true, data, NULL, action_delete_dir_contents_draw_top, action_delete_dir_contents_onresponse));
}