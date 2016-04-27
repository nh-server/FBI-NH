#include <malloc.h>
#include <stdlib.h>

#include "linkedlist.h"

void linked_list_init(linked_list* list) {
    list->first = NULL;
    list->last = NULL;
    list->size = 0;
}

void linked_list_destroy(linked_list* list) {
    linked_list_clear(list);
}

unsigned int linked_list_size(linked_list* list) {
    return list->size;
}

void linked_list_clear(linked_list* list) {
    linked_list_node* node = list->first;
    while(node != NULL) {
        linked_list_node* next = node->next;
        free(node);
        node = next;
    }

    list->first = NULL;
    list->last = NULL;
    list->size = 0;
}

bool linked_list_contains(linked_list* list, void* value) {
    linked_list_node* node = list->first;
    while(node != NULL) {
        if(node->value == value) {
            return true;
        }

        node = node->next;
    }

    return false;
}

static linked_list_node* linked_list_get_node(linked_list* list, unsigned int index) {
    if(index < 0 || index >= list->size) {
        return NULL;
    }

    linked_list_node* node = NULL;

    if(index > (list->size - 1) / 2) {
        node = list->last;
        unsigned int pos = list->size - 1;
        while(node != NULL && pos != index) {
            node = node->prev;
            pos--;
        }
    } else {
        node = list->first;
        unsigned int pos = 0;
        while(node != NULL && pos != index) {
            node = node->next;
            pos++;
        }
    }

    return node;
}

void* linked_list_get(linked_list* list, unsigned int index) {
    linked_list_node* node = linked_list_get_node(list, index);
    return node != NULL ? node->value : NULL;
}

bool linked_list_add(linked_list* list, void* value) {
    linked_list_node* node = (linked_list_node*) calloc(1, sizeof(linked_list_node));
    if(node == NULL) {
        return false;
    }

    node->value = value;
    node->next = NULL;

    if(list->first == NULL || list->last == NULL) {
        node->prev = NULL;

        list->first = node;
        list->last = node;
    } else {
        node->prev = list->last;

        list->last->next = node;
        list->last = node;
    }

    list->size++;
    return true;
}

bool linked_list_add_at(linked_list* list, unsigned int index, void* value) {
    linked_list_node* node = (linked_list_node*) calloc(1, sizeof(linked_list_node));
    if(node == NULL) {
        return false;
    }

    node->value = value;

    if(index == 0) {
        node->prev = NULL;
        node->next = list->first;

        list->first = node;
    } else {
        linked_list_node* prev = linked_list_get_node(list, index - 1);
        if(prev == NULL) {
            free(node);
            return false;
        }

        node->prev = prev;
        node->next = prev->next;

        prev->next = node;
    }

    if(node->next != NULL) {
        node->next->prev = node;
    } else {
        list->last = node;
    }

    list->size++;
    return true;
}

static void linked_list_remove_node(linked_list* list, linked_list_node* node) {
    if(node->prev != NULL) {
        node->prev->next = node->next;
    }

    if(node->next != NULL) {
        node->next->prev = node->prev;
    }

    if(list->first == node) {
        list->first = node->next;
    }

    if(list->last == node) {
        list->last = node->prev;
    }

    list->size--;

    free(node);
}

bool linked_list_remove(linked_list* list, void* value) {
    bool found = false;

    linked_list_node* node = list->first;
    while(node != NULL) {
        linked_list_node* next = node->next;

        if(node->value == value) {
            found = true;

            linked_list_remove_node(list, node);
        }

        node = next;
    }

    return found;
}

bool linked_list_remove_at(linked_list* list, unsigned int index) {
    linked_list_node* node = linked_list_get_node(list, index);
    if(node == NULL) {
        return false;
    }

    linked_list_remove_node(list, node);
    return true;
}

void linked_list_sort(linked_list* list, int (*compare)(const void* p1, const void* p2)) {
    unsigned int count = list->size;
    void* elements[count];

    unsigned int i = 0;
    linked_list_node* node = list->first;
    while(node != NULL && i < count) {
        elements[i++] = node->value;
        node = node->next;
    }

    linked_list_clear(list);

    qsort(elements, count, sizeof(void*), compare);

    for(unsigned int index = 0; index < count; index++) {
        linked_list_add(list, elements[index]);
    }
}

void linked_list_iterate(linked_list* list, linked_list_iter* iter) {
    iter->list = list;
    linked_list_iter_restart(iter);
}

void linked_list_iter_restart(linked_list_iter* iter) {
    if(iter->list == NULL) {
        return;
    }

    iter->curr = NULL;
    iter->next = iter->list->first;
}

bool linked_list_iter_has_next(linked_list_iter* iter) {
    return iter->next != NULL;
}

void* linked_list_iter_next(linked_list_iter* iter) {
    if(iter->next == NULL) {
        return NULL;
    }

    iter->curr = iter->next;
    iter->next = iter->next->next;
    return iter->curr->value;
}

void linked_list_iter_remove(linked_list_iter* iter) {
    if(iter->curr == NULL) {
        return;
    }

    linked_list_remove_node(iter->list, iter->curr);
}