#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "task.h"
#include "../../list.h"
#include "../../error.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"

typedef struct {
    linked_list* items;

    Handle cancelEvent;
} populate_tickets_data;

static void task_populate_tickets_thread(void* arg) {
    populate_tickets_data* data = (populate_tickets_data*) arg;

    Result res = 0;

    u32 ticketCount = 0;
    if(R_SUCCEEDED(res = AM_GetTicketCount(&ticketCount))) {
        u64* ticketIds = (u64*) calloc(ticketCount, sizeof(u64));
        if(ticketIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetTicketList(&ticketCount, ticketCount, 0, ticketIds))) {
                qsort(ticketIds, ticketCount, sizeof(u64), util_compare_u64);

                for(u32 i = 0; i < ticketCount && R_SUCCEEDED(res); i++) {
                    svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                        break;
                    }

                    list_item* item = (list_item*) calloc(1, sizeof(list_item));
                    if(item != NULL) {
                        ticket_info* ticketInfo = (ticket_info*) calloc(1, sizeof(ticket_info));
                        if(ticketInfo != NULL) {
                            ticketInfo->titleId = ticketIds[i];

                            snprintf(item->name, LIST_ITEM_NAME_MAX, "%016llX", ticketIds[i]);
                            item->color = COLOR_TEXT;
                            item->data = ticketInfo;

                            linked_list_add(data->items, item);
                        } else {
                            free(item);

                            res = R_FBI_OUT_OF_MEMORY;
                        }
                    } else {
                        res = R_FBI_OUT_OF_MEMORY;
                    }
                }
            }

            free(ticketIds);
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }
    }

    if(R_FAILED(res)) {
        error_display_res(NULL, NULL, NULL, res, "Failed to load ticket listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

void task_free_ticket(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        free(item->data);
    }

    free(item);
}

void task_clear_tickets(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);
        task_free_ticket(item);
        linked_list_iter_remove(&iter);
    }
}

Handle task_populate_tickets(linked_list* items) {
    if(items == NULL) {
        return 0;
    }

    task_clear_tickets(items);

    populate_tickets_data* data = (populate_tickets_data*) calloc(1, sizeof(populate_tickets_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate ticket list data.");

        return 0;
    }

    data->items = items;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create ticket list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_tickets_thread, data, 0x10000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create ticket list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}