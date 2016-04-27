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
} delete_ticket_data;

static void action_delete_ticket_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_ticket_info(view, ((delete_ticket_data*) data)->selected->data, x1, y1, x2, y2);
}

static void action_delete_ticket_update(ui_view* view, void* data, float* progress, char* text) {
    delete_ticket_data* deleteData = (delete_ticket_data*) data;

    ticket_info* info = (ticket_info*) deleteData->selected->data;

    Result res = AM_DeleteTicket(info->titleId);

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(NULL, info, ui_draw_ticket_info, res, "Failed to delete ticket.");
    } else {
        linked_list_remove(deleteData->items, deleteData->selected);
        task_free_ticket(deleteData->selected);

        prompt_display("Success", "Ticket deleted.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
    }

    free(data);
}

static void action_delete_ticket_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        info_display("Deleting Ticket", "", false, data, action_delete_ticket_update, action_delete_ticket_draw_top);
    } else {
        free(data);
    }
}

void action_delete_ticket(linked_list* items, list_item* selected) {
    delete_ticket_data* data = (delete_ticket_data*) calloc(1, sizeof(delete_ticket_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate delete ticket data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    prompt_display("Confirmation", "Delete the selected ticket?", COLOR_TEXT, true, data, NULL, action_delete_ticket_draw_top, action_delete_ticket_onresponse);
}