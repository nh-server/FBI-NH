#include <malloc.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../info.h"
#include "../../prompt.h"
#include "../../../screen.h"

typedef struct {
    title_info* info;
    bool* populated;
} delete_title_data;

static void action_delete_title_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_title_info(view, ((delete_title_data*) data)->info, x1, y1, x2, y2);
}

static void action_delete_title_update(ui_view* view, void* data, float* progress, char* text) {
    delete_title_data* deleteData = (delete_title_data*) data;

    Result res = AM_DeleteTitle(deleteData->info->mediaType, deleteData->info->titleId);

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(NULL, deleteData->info, ui_draw_title_info, res, "Failed to delete title.");
    } else {
        *deleteData->populated = false;

        prompt_display("Success", "Title deleted.", COLOR_TEXT, false, deleteData->info, NULL, ui_draw_title_info, NULL);
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

void action_delete_title(title_info* info, bool* populated) {
    delete_title_data* data = (delete_title_data*) calloc(1, sizeof(delete_title_data));
    data->info = info;
    data->populated = populated;

    prompt_display("Confirmation", "Delete the selected title?", COLOR_TEXT, true, data, NULL, action_delete_title_draw_top, action_delete_title_onresponse);
}