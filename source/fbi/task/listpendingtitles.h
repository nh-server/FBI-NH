#pragma once

typedef struct linked_list_s linked_list;
typedef struct list_item_s list_item;

typedef struct pending_title_info_s {
    FS_MediaType mediaType;
    u64 titleId;
    u16 version;
} pending_title_info;

typedef struct populate_pending_titles_data_s {
    linked_list* items;

    volatile bool finished;
    Result result;
    Handle cancelEvent;
} populate_pending_titles_data;

void task_free_pending_title(list_item* item);
void task_clear_pending_titles(linked_list* items);
Result task_populate_pending_titles(populate_pending_titles_data* data);