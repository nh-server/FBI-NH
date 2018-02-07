#pragma once

typedef struct linked_list_s linked_list;
typedef struct list_item_s list_item;

typedef struct title_info_s {
    FS_MediaType mediaType;
    u64 titleId;
    char productCode[0x10];
    u16 version;
    u64 installedSize;
    bool twl;
    bool hasMeta;
    meta_info meta;
} title_info;

typedef struct populate_titles_data_s {
    linked_list* items;

    void* userData;
    bool (*filter)(void* data, u64 titleId, FS_MediaType mediaType);
    int (*compare)(void* data, const void* p1, const void* p2);

    volatile bool finished;
    Result result;
    Handle cancelEvent;
} populate_titles_data;

void task_free_title(list_item* item);
void task_clear_titles(linked_list* items);
Result task_populate_titles(populate_titles_data* data);