#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "task/task.h"
#include "section.h"

#define TICKETS_MAX 1024

typedef struct {
    list_item items[TICKETS_MAX];
    u32 count;
    Handle cancelEvent;
    bool populated;
} tickets_data;

#define TICKETS_ACTION_COUNT 1

static u32 tickets_action_count = TICKETS_ACTION_COUNT;
static list_item tickets_action_items[TICKETS_ACTION_COUNT] = {
        {"Delete Ticket", 0xFF000000, action_delete_ticket},
};

typedef struct {
    ticket_info* info;
    bool* populated;
} tickets_action_data;

static void tickets_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_ticket_info(view, ((tickets_action_data*) data)->info, x1, y1, x2, y2);
}

static void tickets_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    tickets_action_data* actionData = (tickets_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(ticket_info*, bool*) = (void(*)(ticket_info*, bool*)) selected->data;

        list_destroy(view);
        ui_pop();

        action(actionData->info, actionData->populated);

        free(data);

        return;
    }

    if(*itemCount != &tickets_action_count || *items != tickets_action_items) {
        *itemCount = &tickets_action_count;
        *items = tickets_action_items;
    }
}

static ui_view* tickets_action_create(ticket_info* info, bool* populated) {
    tickets_action_data* data = (tickets_action_data*) calloc(1, sizeof(tickets_action_data));
    data->info = info;
    data->populated = populated;

    return list_create("Ticket Action", "A: Select, B: Return", data, tickets_action_update, tickets_action_draw_top);
}

static void tickets_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ticket_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void tickets_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
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
        free(listData);
        list_destroy(view);
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

        listData->cancelEvent = task_populate_tickets(listData->items, &listData->count, TICKETS_MAX);
        listData->populated = true;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        ui_push(tickets_action_create((ticket_info*) selected->data, &listData->populated));
        return;
    }

    if(*itemCount != &listData->count || *items != listData->items) {
        *itemCount = &listData->count;
        *items = listData->items;
    }
}

void tickets_open() {
    tickets_data* data = (tickets_data*) calloc(1, sizeof(tickets_data));

    ui_push(list_create("Tickets", "A: Select, B: Return, X: Refresh", data, tickets_update, tickets_draw_top));
}