/* Copyright (c) 2017 imv authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "list.h"

struct imv_list *imv_list_create(void)
{
  struct imv_list *list = malloc(sizeof(struct imv_list));
  list->len = 0;
  list->cap = 64;
  list->items = malloc(sizeof(void*) * list->cap);
  return list;
}

void imv_list_free(struct imv_list *list)
{
  free(list->items);
}

void imv_list_deep_free(struct imv_list *list)
{
  for(size_t i = 0; i < list->len; ++i) {
    free(list->items[i]);
  }
  imv_list_free(list);
}

void imv_list_append(struct imv_list *list, void *item)
{
  imv_list_grow(list, list->len + 1);
  list->items[list->len++] = item;
}

void imv_list_grow(struct imv_list *list, size_t min_size)
{
  if(list->cap >= min_size) {
    return;
  }

  while(list->cap < min_size) {
    list->cap *= 2;
  }

  list->items = realloc(list->items, sizeof(void*) * list->cap);
}

void imv_list_remove(struct imv_list *list, size_t index)
{
  if(index >= list->len) {
    return;
  }

  memmove(&list->items[index], &list->items[index + 1], list->len - index);

  list->len -= 1;
}

void imv_list_insert(struct imv_list *list, size_t index, void *item)
{
  imv_list_grow(list, list->len + 1);

  if(index > list->len) {
    index = list->len;
  }

  memmove(&list->items[index + 1], &list->items[index], list->len - index);
  list->items[index] = item;
  list->len += 1;
}

struct imv_list *imv_split_string(const char *string, char delim)
{
  struct imv_list *list = imv_list_create();

  const char *base = string;

  while(*base) {
    while(*base && *base == delim) {
      ++base;
    }

    const char *end = base;
    while(*end && *end != delim) {
      ++end;
    }

    if(*base && base != end) {
      char *item = strndup(base, end - base);
      imv_list_append(list, item);
      base = end;
    }
  }

  return list;
}

/* vim:set ts=2 sts=2 sw=2 et: */
