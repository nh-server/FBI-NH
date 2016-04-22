#include <sys/syslimits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "../../list.h"
#include "../../error.h"
#include "../../../screen.h"
#include "../../../util.h"
#include "task.h"

typedef struct {
    list_item* items;
    u32* count;
    u32 max;

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

                for(u32 i = 0; i < ticketCount && i < data->max && R_SUCCEEDED(res); i++) {
                    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                        break;
                    }

                    ticket_info* ticketInfo = (ticket_info*) calloc(1, sizeof(ticket_info));
                    if(ticketInfo != NULL) {
                        ticketInfo->ticketId = ticketIds[i];

                        list_item* item = &data->items[*data->count];
                        snprintf(item->name, NAME_MAX, "%016llX", ticketIds[i]);
                        item->rgba = COLOR_TEXT;
                        item->data = ticketInfo;

                        (*data->count)++;
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

void task_clear_tickets(list_item* items, u32* count) {
    if(items == NULL || count == NULL) {
        return;
    }

    u32 prevCount = *count;
    *count = 0;

    for(u32 i = 0; i < prevCount; i++) {
        if(items[i].data != NULL) {
            free(items[i].data);
            items[i].data = NULL;
        }

        memset(items[i].name, '\0', NAME_MAX);
        items[i].rgba = 0;
    }
}

Handle task_populate_tickets(list_item* items, u32* count, u32 max) {
    if(items == NULL || count == NULL || max == 0) {
        return 0;
    }

    task_clear_tickets(items, count);

    populate_tickets_data* data = (populate_tickets_data*) calloc(1, sizeof(populate_tickets_data));
    data->items = items;
    data->count = count;
    data->max = max;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create ticket list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_tickets_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create ticket list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}