#pragma once

#include <stdbool.h>

typedef struct linked_list_node_s {
    struct linked_list_node_s* prev;
    struct linked_list_node_s* next;
    void* value;
} linked_list_node;

typedef struct linked_list_s {
    linked_list_node* first;
    linked_list_node* last;
    unsigned int size;
} linked_list;

typedef struct linked_list_iter_s {
    linked_list* list;
    linked_list_node* curr;
    linked_list_node* next;
} linked_list_iter;

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);

unsigned int linked_list_size(linked_list* list);
void linked_list_clear(linked_list* list);
bool linked_list_contains(linked_list* list, void* value);
void* linked_list_get(linked_list* list, unsigned int index);
bool linked_list_add(linked_list* list, void* value);
bool linked_list_add_at(linked_list* list, unsigned int index, void* value);
bool linked_list_remove(linked_list* list, void* value);
bool linked_list_remove_at(linked_list* list, unsigned int index);
void linked_list_sort(linked_list* list, int (*compare)(const void* p1, const void* p2));

void linked_list_iterate(linked_list* list, linked_list_iter* iter);

void linked_list_iter_restart(linked_list_iter* iter);
bool linked_list_iter_has_next(linked_list_iter* iter);
void* linked_list_iter_next(linked_list_iter* iter);
void linked_list_iter_remove(linked_list_iter* iter);