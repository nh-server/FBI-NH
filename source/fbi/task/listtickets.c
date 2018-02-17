#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "listtickets.h"
#include "../resources.h"
#include "../../core/core.h"

static int task_populate_tickets_compare_ids(const void* e1, const void* e2) {
    u64 id1 = *(u64*) e1;
    u64 id2 = *(u64*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

void task_populate_tickets_update_use(list_item* item) {
    if(item == NULL) {
        return;
    }

    ticket_info* info = (ticket_info*) item->data;

    info->inUse = false;

    AM_TitleEntry entry;
    for(FS_MediaType mediaType = MEDIATYPE_NAND; mediaType != MEDIATYPE_GAME_CARD; mediaType++) {
        if(R_SUCCEEDED(AM_GetTitleInfo(mediaType, 1, &info->titleId, &entry))) {
            info->inUse = true;
            break;
        }
    }

    item->color = info->inUse ? COLOR_TICKET_IN_USE : COLOR_TICKET_NOT_IN_USE;
}

static void task_populate_tickets_thread(void* arg) {
    populate_tickets_data* data = (populate_tickets_data*) arg;

    Result res = 0;

    u32 ticketCount = 0;
    if(R_SUCCEEDED(res = AM_GetTicketCount(&ticketCount))) {
        u64* ticketIds = (u64*) calloc(ticketCount, sizeof(u64));
        if(ticketIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetTicketList(&ticketCount, ticketCount, 0, ticketIds))) {
                qsort(ticketIds, ticketCount, sizeof(u64), task_populate_tickets_compare_ids);

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
                            ticketInfo->loaded = true;

                            snprintf(item->name, LIST_ITEM_NAME_MAX, "%016llX", ticketIds[i]);
                            item->data = ticketInfo;

                            task_populate_tickets_update_use(item);

                            linked_list_add(data->items, item);
                        } else {
                            free(item);

                            res = R_APP_OUT_OF_MEMORY;
                        }
                    } else {
                        res = R_APP_OUT_OF_MEMORY;
                    }
                }
            }

            free(ticketIds);
        } else {
            res = R_APP_OUT_OF_MEMORY;
        }
    }

    svcCloseHandle(data->cancelEvent);

    data->result = res;
    data->finished = true;
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

        linked_list_iter_remove(&iter);
        task_free_ticket(item);
    }
}

Result task_populate_tickets(populate_tickets_data* data) {
    if(data == NULL || data->items == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    task_clear_tickets(data->items);

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(threadCreate(task_populate_tickets_thread, data, 0x10000, 0x19, 1, true) == NULL) {
            res = R_APP_THREAD_CREATE_FAILED;
        }
    }

    if(R_FAILED(res)) {
        data->finished = true;

        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }
    }

    return res;
}