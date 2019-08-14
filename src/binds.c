#include "binds.h"
#include "list.h"

#include <stdbool.h>
#include <stdio.h>

struct bind_node {
  char *key; /* input key to reach this node */
  struct list *commands; /* commands to run for this node, or NULL if not leaf node */
  struct list *suffixes; /* list of bind_node* suffixes, or NULL if leaf node */
};

struct imv_binds {
  struct bind_node bind_tree;
  struct list *keys;
  bool aborting_sequence;
};

static int compare_node_key(const void* item, const void *key)
{
  const struct bind_node *node = item;
  const char *keystr = key;
  return strcmp(node->key, keystr);
}

static void init_bind_node(struct bind_node *bn)
{
  bn->key = NULL;
  bn->commands = NULL;
  bn->suffixes = list_create();
}

static void destroy_bind_node(struct bind_node *bn)
{
  for(size_t i = 0; i < bn->suffixes->len; ++i) {
    destroy_bind_node(bn->suffixes->items[i]);
  }
  free(bn->key);
  if(bn->commands) {
    list_deep_free(bn->commands);
  }
  list_deep_free(bn->suffixes);
}

struct imv_binds *imv_binds_create(void)
{
  struct imv_binds *binds = calloc(1, sizeof *binds);
  init_bind_node(&binds->bind_tree);
  binds->keys = list_create();
  return binds;
}

void imv_binds_free(struct imv_binds *binds)
{
  destroy_bind_node(&binds->bind_tree);
  list_deep_free(binds->keys);
  free(binds);
}

enum bind_result imv_binds_add(struct imv_binds *binds, const struct list *keys, const char *command)
{
  if(!command) {
    return BIND_INVALID_COMMAND;
  }

  if(!keys) {
    return BIND_INVALID_KEYS;
  }

  /* Prepare our return code */
  int result = BIND_SUCCESS;

  /* Traverse the trie */
  struct bind_node *node = &binds->bind_tree;
  for(size_t i = 0; i < keys->len; ++i) {
    /* If we've reached a node that already has a command, there's a conflict */
    if(node->commands) {
      result = BIND_INVALID_COMMAND;
      break;
    }

    /* Find / create a child with the current key */
    struct bind_node *next_node = NULL;
    int child_index = list_find(node->suffixes, &compare_node_key, keys->items[i]);
    if(child_index == -1) {
      /* Create our new node */
      next_node = malloc(sizeof *next_node);
      init_bind_node(next_node);
      next_node->key = strdup(keys->items[i]);
      list_append(node->suffixes, next_node);
    } else {
      next_node = node->suffixes->items[child_index];
    }

    /* We've now found the correct node for this key */

    /* Check if the node has a command */
    if(next_node->commands) {
      if(i + 1 < keys->len) {
        /* If we're not at the end, it's a conflict */
        result = BIND_CONFLICTS;
        break;
      } else {
        /* Otherwise we just need to append a new command to the existing bind. */
        list_append(next_node->commands, strdup(command));
        result = BIND_SUCCESS;
        break;
      }
    }

    if(i + 1 == keys->len) {
      /* this is the last key part, try to insert command */
      /* but first, make sure there's no children */
      if(next_node->suffixes->len > 0) {
        result = BIND_CONFLICTS;
      } else {
        next_node->commands = list_create();
        list_append(next_node->commands, strdup(command));
      }
    } else {
      /* Otherwise, move down the trie */
      node = next_node;
    }
  }

  return result;
}

void imv_binds_clear(struct imv_binds *binds)
{
  destroy_bind_node(&binds->bind_tree);
  init_bind_node(&binds->bind_tree);
}

void imv_binds_clear_key(struct imv_binds *binds, const struct list *keys)
{
  struct bind_node *node = &binds->bind_tree;

  for(size_t i = 0; i < keys->len; ++i) {
    /* Traverse the trie to find the right node for the input keys */
    int child_index = list_find(node->suffixes, &compare_node_key, keys->items[i]);
    if(child_index == -1) {
      /* No such node, no more work to do */
      return;
    } else {
      node = node->suffixes->items[child_index];
    }
  }

  /* We've now found the correct node for the input */

  /* Clear the commands for this node */
  if(node->commands) {
    list_deep_free(node->commands);
    node->commands = NULL;
  }
}

enum lookup_result {
  LOOKUP_PARTIAL,
  LOOKUP_INVALID,
  LOOKUP_MATCH,
};

static enum lookup_result bind_lookup(struct bind_node *node, struct list *keys, struct list **out_list)
{
  for(size_t part = 0; part < keys->len; ++part) {
    const char* cur_key = keys->items[part];
    int found = 0;
    for(size_t i = 0; i < node->suffixes->len; ++i) {
      struct bind_node* cur_node = node->suffixes->items[i];
      if(strcmp(cur_node->key, cur_key) == 0) {
        node = node->suffixes->items[i];
        found = 1;
        break;
      }
    }
    if(!found) {
      return LOOKUP_INVALID;
    }
  }

  if(node->commands) {
    *out_list = node->commands;
    return LOOKUP_MATCH;
  }
  return LOOKUP_PARTIAL;
}

void imv_bind_clear_input(struct imv_binds *binds)
{
  list_deep_free(binds->keys);
  binds->keys = list_create();
}

struct list *imv_bind_handle_event(struct imv_binds *binds, const char *event)
{
  /* If the user hits Escape twice in a row, treat that as backtracking out
   * of the current key sequence. */
  if (!strcmp("Escape", event)) {
    if (binds->aborting_sequence) {
      /* The last thing they hit was escape, so abort the current entry */
      binds->aborting_sequence = false;
      imv_bind_clear_input(binds);
      return NULL;
    } else {
      /* Start the aborting sequence */
      binds->aborting_sequence = true;
    }
  } else {
    /* They didn't hit escape, if they were in an abort sequence, cancel it */
    binds->aborting_sequence = false;
  }

  list_append(binds->keys, strdup(event));

  struct list *commands = NULL;
  enum lookup_result result = bind_lookup(&binds->bind_tree, binds->keys, &commands);
  if(result == LOOKUP_PARTIAL) {
    return NULL;
  } else if(result == LOOKUP_MATCH) {
    imv_bind_clear_input(binds);
    return commands;
  } else if(result == LOOKUP_INVALID) {
    imv_bind_clear_input(binds);
    return NULL;
  }

  /* Should not happen */
  imv_bind_clear_input(binds);
  return NULL;
}

struct list *imv_bind_parse_keys(const char *keys)
{
  struct list *list = list_create();

  /* Iterate through the string, breaking it into its parts */
  while(*keys) {

    if(*keys == '<') {
      /* Keyname block, need to extract the name, and convert it */
      const char *end = keys;
      while(*end && *end != '>') {
        ++end;
      }
      if(*end == '>') {
        /* We've got a <stuff> block. Check if it's a valid special key name. */
        const size_t key_len = end - keys;
        char *key = malloc(key_len);
        memcpy(key, keys + 1 /* skip the '<' */, key_len - 1);
        key[key_len - 1] = 0;
        list_append(list, key);
        keys = end + 1;
      } else {
        /* A <key> block that didn't have a closing '<'. Abort. */
        list_deep_free(list);
        return NULL;
      }
    } else {
      /* Just a regular character */
      char *item = malloc(2);
      item[0] = *keys;
      item[1] = 0;
      list_append(list, item);
      ++keys;
    }
  }

  return list;
}

size_t imv_bind_print_keylist(const struct list *keys, char *buf, size_t len)
{
  size_t printed = 0;

  /* iterate through all the keys, wrapping them in '<' and '>' if needed */
  for(size_t i = 0; i < keys->len; ++i) {
    const char *key = keys->items[i];
    const char *format = strlen(key) > 1 ? "<%s>" : "%s";
    printed += snprintf(buf, len - printed, format, key);
  }
  return printed;
}
