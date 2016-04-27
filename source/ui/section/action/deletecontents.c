#include <malloc.h>
#include <stdio.h>
#include <string.h>

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
#include "../../../core/util.h"

typedef struct {
    linked_list* items;
    file_info* target;

    char** contents;
    list_item* curr;

    data_op_info deleteInfo;
    Handle cancelEvent;
} delete_contents_data;

static Result action_delete_contents_delete(void* data, u32 index) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    list_item* old = deleteData->curr;
    task_create_file_item(&deleteData->curr, deleteData->target->archive, deleteData->contents[index]);
    if(old != NULL) {
        task_free_file(old);
    }

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(deleteData->contents[index]);
    if(fsPath != NULL) {
        if(util_is_dir(deleteData->target->archive, deleteData->contents[index])) {
            res = FSUSER_DeleteDirectory(*deleteData->target->archive, *fsPath);
        } else {
            res = FSUSER_DeleteFile(*deleteData->target->archive, *fsPath);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    if(R_SUCCEEDED(res)) {
        linked_list_iter iter;
        linked_list_iterate(deleteData->items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            list_item* item = (list_item*) linked_list_iter_next(&iter);
            file_info* currInfo = (file_info*) item->data;

            if(strcmp(currInfo->path, deleteData->contents[index]) == 0) {
                linked_list_iter_remove(&iter);
                break;
            }
        }
    }

    return res;
}

static bool action_delete_contents_error(void* data, u32 index, Result res) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Delete cancelled.", COLOR_TEXT, false, deleteData->target, NULL, ui_draw_file_info, NULL);
        return false;
    } else {
        volatile bool dismissed = false;
        error_display_res(&dismissed, deleteData->curr != NULL ? deleteData->curr->data : deleteData->target, ui_draw_file_info, res, "Failed to delete content.");

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < deleteData->deleteInfo.total - 1;
}

static void action_delete_contents_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(deleteData->curr != NULL) {
        ui_draw_file_info(view, ((delete_contents_data*) data)->curr->data, x1, y1, x2, y2);
    } else if(deleteData->target != NULL) {
        ui_draw_file_info(view, ((delete_contents_data*) data)->target, x1, y1, x2, y2);
    }
}

static void action_delete_contents_free_data(delete_contents_data* data) {
    if(data->curr != NULL) {
        task_free_file(data->curr);
        data->curr = NULL;
    }

    util_free_contents(data->contents, data->deleteInfo.total);
    free(data);
}

static void action_delete_contents_update(ui_view* view, void* data, float* progress, char* text) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(deleteData->deleteInfo.finished) {
        if(deleteData->target->archive->id == ARCHIVE_USER_SAVEDATA) {
            FSUSER_ControlArchive(*deleteData->target->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
        }

        ui_pop();
        info_destroy(view);

        if(!deleteData->deleteInfo.premature) {
            prompt_display("Success", "Contents deleted.", COLOR_TEXT, false, deleteData->target, NULL, ui_draw_file_info, NULL);
        }

        action_delete_contents_free_data(deleteData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(deleteData->cancelEvent);
    }

    *progress = deleteData->deleteInfo.total > 0 ? (float) deleteData->deleteInfo.processed / (float) deleteData->deleteInfo.total : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu", deleteData->deleteInfo.processed, deleteData->deleteInfo.total);
}

static void action_delete_contents_onresponse(ui_view* view, void* data, bool response) {
    delete_contents_data* deleteData = (delete_contents_data*) data;

    if(response) {
        deleteData->cancelEvent = task_data_op(&deleteData->deleteInfo);
        if(deleteData->cancelEvent != 0) {
            info_display("Deleting Contents", "Press B to cancel.", true, data, action_delete_contents_update, action_delete_contents_draw_top);
        } else {
            error_display(NULL, NULL, NULL, "Failed to initiate delete operation.");
        }
    } else {
        action_delete_contents_free_data(deleteData);
    }
}

static void action_delete_contents_internal(linked_list* items, list_item* selected, file_info* target, const char* message, bool recursive, void* filterData, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    delete_contents_data* data = (delete_contents_data*) calloc(1, sizeof(delete_contents_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate delete contents data.");

        return;
    }

    data->items = items;
    data->target = target;

    data->deleteInfo.data = data;

    data->deleteInfo.op = DATAOP_DELETE;

    data->deleteInfo.delete = action_delete_contents_delete;

    data->deleteInfo.error = action_delete_contents_error;

    data->cancelEvent = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->deleteInfo.total, target->archive, target->path, recursive, false, filterData, filter))) {
        error_display_res(NULL, target, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    prompt_display("Confirmation", message, COLOR_TEXT, true, data, NULL, action_delete_contents_draw_top, action_delete_contents_onresponse);
}

void action_delete_contents(linked_list* items, list_item* selected, file_info* target) {
    action_delete_contents_internal(items, selected, target, "Delete the selected content?", true, NULL, NULL);
}

void action_delete_dir(linked_list* items, list_item* selected, file_info* target) {
    action_delete_contents_internal(items, selected, target, "Delete the current directory?", true, NULL, NULL);
}

void action_delete_dir_contents(linked_list* items, list_item* selected, file_info* target) {
    action_delete_contents_internal(items, selected, target, "Delete all contents of the current directory?", true, target->path, util_filter_not_path);
}

void action_delete_dir_cias(linked_list* items, list_item* selected, file_info* target) {
    action_delete_contents_internal(items, selected, target, "Delete all CIAs in the current directory?", false, ".cia", util_filter_file_extension);
}

void action_delete_dir_tickets(linked_list* items, list_item* selected, file_info* target) {
    action_delete_contents_internal(items, selected, target, "Delete all tickets in the current directory?", false, ".tik", util_filter_file_extension);
}