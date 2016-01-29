#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"

#define TICKETS_ACTION_COUNT 1

static u32 tickets_action_count = TICKETS_ACTION_COUNT;
static list_item tickets_action_items[TICKETS_ACTION_COUNT] = {
        {"Delete Ticket", 0xFF000000, action_delete_ticket},
};

static void tickets_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_ticket_info(view, data, x1, y1, x2, y2);
}

static void tickets_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(ticket_info*) = (void(*)(ticket_info*)) selected->data;

        list_destroy(view);
        ui_pop();

        action((ticket_info*) data);
        return;
    }

    if(*itemCount != &tickets_action_count || *items != tickets_action_items) {
        *itemCount = &tickets_action_count;
        *items = tickets_action_items;
    }
}

static ui_view* tickets_action_create(ticket_info* info) {
    return list_create("Ticket Action", "A: Select, B: Return", info, tickets_action_update, tickets_action_draw_top);
}

static void tickets_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ticket_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void tickets_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(hidKeysDown() & KEY_X) {
        task_refresh_tickets();
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        ui_push(tickets_action_create((ticket_info*) selected->data));
    }

    if(*itemCount != task_get_ticket_count() || *items != task_get_tickets()) {
        *itemCount = task_get_ticket_count();
        *items = task_get_tickets();
    }
}

void tickets_open() {
    ui_push(list_create("Tickets", "A: Select, B: Return, X: Refresh", NULL, tickets_update, tickets_draw_top));
}