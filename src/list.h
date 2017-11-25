#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <string.h>

struct list {
  size_t len;
  size_t cap;
  void **items;
};

struct list *list_create(void);

void list_free(struct list *list);

void list_deep_free(struct list *list);

void list_append(struct list *list, void *item);

void list_grow(struct list *list, size_t min_size);

void list_remove(struct list *list, size_t index);

void list_insert(struct list *list, size_t index, void *item);

struct list *list_from_string(const char *string, char delim);

int list_find(
    struct list *list,
    int (*cmp)(const void *item, const void *key),
    const void *key
);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
