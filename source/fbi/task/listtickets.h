#pragma once

typedef struct linked_list_s linked_list;
typedef struct list_item_s list_item;

typedef struct ticket_info_s {
    bool loaded;

    u64 titleId;
    bool inUse;
} ticket_info;

typedef struct populate_tickets_data_s {
    linked_list* items;

    volatile bool finished;
    Result result;
    Handle cancelEvent;
} populate_tickets_data;

void task_populate_tickets_update_use(list_item* item);
void task_free_ticket(list_item* item);
void task_clear_tickets(linked_list* items);
Result task_populate_tickets(populate_tickets_data* data);