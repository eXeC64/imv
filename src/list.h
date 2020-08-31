#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <string.h>

/* Generic list. You know what this is. */
struct list {
  size_t len;
  size_t cap;
  void **items;
};

/* Create a list instance */
struct list *list_create(void);

/* Clean up a list, caller should clean up contents of the list first */
void list_free(struct list *list);

/* Clean up a list, calling free() on each item first */
void list_deep_free(struct list *list);

/* Append an item to the list. Automatically resizes the list if needed */
void list_append(struct list *list, void *item);

/* Grow the list's storage to a given size, useful for avoiding unnecessary
 * reallocations prior to inserting many items
 */
void list_grow(struct list *list, size_t min_size);

/* Remove an item from the list at a given 0-based index */
void list_remove(struct list *list, size_t index);

/* Insert an item into the list before the given index */
void list_insert(struct list *list, size_t index, void *item);

/* Empty the list. Caller should clean up the contents of the list first */
void list_clear(struct list *list);

/* Split a null-terminated string, separating by the given delimiter.
 * Multiple consecutive delimiters count as a single delimiter, so no empty
 * string list items are emitted
 */
struct list *list_from_string(const char *string, char delim);

/* Returns the index of the first item to match key, determined by the cmp
 * function
 */
int list_find(
    struct list *list,
    int (*cmp)(const void *item, const void *key),
    const void *key
);

/* Assumes all list items are null-terminated strings, and concatenates them
 * separated by sep, starting at the index given by start
 */
char *list_to_string(struct list *list, const char *sep, size_t start);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
