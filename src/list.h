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

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
