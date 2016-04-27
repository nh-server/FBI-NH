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
} delete_title_data;

static void action_delete_title_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_title_info(view, ((delete_title_data*) data)->selected->data, x1, y1, x2, y2);
}

static void action_delete_title_update(ui_view* view, void* data, float* progress, char* text) {
    delete_title_data* deleteData = (delete_title_data*) data;

    title_info* info = (title_info*) deleteData->selected->data;

    Result res = AM_DeleteTitle(info->mediaType, info->titleId);

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(NULL, info, ui_draw_title_info, res, "Failed to delete title.");
    } else {
        linked_list_remove(deleteData->items, deleteData->selected);
        task_free_title(deleteData->selected);

        prompt_display("Success", "Title deleted.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
    }

    free(data);
}

static void action_delete_title_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        info_display("Deleting Title", "", false, data, action_delete_title_update, action_delete_title_draw_top);
    } else {
        free(data);
    }
}

void action_delete_title(linked_list* items, list_item* selected) {
    delete_title_data* data = (delete_title_data*) calloc(1, sizeof(delete_title_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate delete title data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    prompt_display("Confirmation", "Delete the selected title?", COLOR_TEXT, true, data, NULL, action_delete_title_draw_top, action_delete_title_onresponse);
}