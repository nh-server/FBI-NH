#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "section.h"
#include "action/action.h"
#include "task/task.h"
#include "../error.h"
#include "../list.h"
#include "../ui.h"
#include "../../core/linkedlist.h"
#include "../../core/screen.h"

static list_item install_from_cdn = {"Install from CDN", COLOR_TEXT, action_install_cdn};
static list_item delete_ticket = {"Delete Ticket", COLOR_TEXT, action_delete_ticket};

typedef struct {
    Handle cancelEvent;
    bool populated;
} tickets_data;

typedef struct {
    linked_list* items;
    list_item* selected;
} tickets_action_data;

static void tickets_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_ticket_info(view, ((tickets_action_data*) data)->selected->data, x1, y1, x2, y2);
}

static void tickets_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    tickets_action_data* actionData = (tickets_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(linked_list*, list_item*) = (void(*)(linked_list*, list_item*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->items, actionData->selected);

        free(data);

        return;
    }

    if(linked_list_size(items) == 0) {
        linked_list_add(items, &install_from_cdn);
        linked_list_add(items, &delete_ticket);
    }
}

static void tickets_action_open(linked_list* items, list_item* selected) {
    tickets_action_data* data = (tickets_action_data*) calloc(1, sizeof(tickets_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate tickets action data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    list_display("Ticket Action", "A: Select, B: Return", data, tickets_action_update, tickets_action_draw_top);
}

static void tickets_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ticket_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void tickets_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    tickets_data* listData = (tickets_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(listData->cancelEvent != 0) {
            svcSignalEvent(listData->cancelEvent);
            while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
                svcSleepThread(1000000);
            }

            listData->cancelEvent = 0;
        }

        ui_pop();

        task_clear_tickets(items);
        list_destroy(view);

        free(listData);
        return;
    }

    if(!listData->populated || (hidKeysDown() & KEY_X)) {
        if(listData->cancelEvent != 0) {
            svcSignalEvent(listData->cancelEvent);
            while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
                svcSleepThread(1000000);
            }

            listData->cancelEvent = 0;
        }

        listData->cancelEvent = task_populate_tickets(items);
        listData->populated = true;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        tickets_action_open(items, selected);
        return;
    }
}

void tickets_open() {
    tickets_data* data = (tickets_data*) calloc(1, sizeof(tickets_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate tickets data.");

        return;
    }

    list_display("Tickets", "A: Select, B: Return, X: Refresh", data, tickets_update, tickets_draw_top);
}