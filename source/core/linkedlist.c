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
    return linked_list_index_of(list, value) != -1;
}

int linked_list_index_of(linked_list* list, void* value) {
    int i = 0;
    linked_list_node* node = list->first;
    while(node != NULL) {
        if(node->value == value) {
            return i;
        }

        i++;
        node = node->next;
    }

    return -1;
}

static linked_list_node* linked_list_get_node(linked_list* list, unsigned int index) {
    if(index >= list->size) {
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

void linked_list_add_sorted(linked_list* list, void* value, void* userData, int (*compare)(void* userData, const void* p1, const void* p2)) {
    if(compare != NULL) {
        unsigned int i = 0;
        linked_list_node* node = list->first;
        while(node != NULL) {
            if(compare(userData, value, node->value) < 0) {
                linked_list_add_at(list, i, value);
                return;
            }

            i++;
            node = node->next;
        }
    }

    linked_list_add(list, value);
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

void linked_list_sort(linked_list* list, void* userData, int (*compare)(void* userData, const void* p1, const void* p2)) {
    bool swapped = true;
    while(swapped) {
        swapped = false;

        linked_list_node* curr = list->first;
        if(curr == NULL) {
            return;
        }

        linked_list_node* next = NULL;
        while((next = curr->next) != NULL) {
            if(compare(userData, curr->value, next->value) > 0) {
                void* temp = curr->value;
                curr->value = next->value;
                next->value = temp;

                swapped = true;
            }

            curr = next;
        }
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
    return iter->next != NULL && iter->next->value != NULL;
}

void* linked_list_iter_next(linked_list_iter* iter) {
    linked_list_node* next = iter->next;
    if(next == NULL) {
        return NULL;
    }

    void* value = next->value;
    if(value == NULL) {
        return NULL;
    }

    iter->curr = next;
    iter->next = next->next;
    return value;
}

void linked_list_iter_remove(linked_list_iter* iter) {
    if(iter->curr == NULL) {
        return;
    }

    linked_list_remove_node(iter->list, iter->curr);
    iter->curr = NULL;
}
