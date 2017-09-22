#ifndef INI_H
#define INI_H

#include <stdio.h>

#define INI_UNKNOWN 0
#define INI_VALUE 1
#define INI_SECTION 2

int parse_ini_file(FILE* f, char *out_key, size_t key_size, char *out_value, size_t value_size);
int parse_ini_str(const char* str, char *out_key, size_t key_size, char *out_value, size_t value_size);

#endif
