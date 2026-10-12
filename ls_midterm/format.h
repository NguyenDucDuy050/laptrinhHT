#ifndef FORMAT_H
#define FORMAT_H

#include "options.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

void format_mode(mode_t mode, char out[11]);
void format_name(const char *name, const Options *options);
void format_size(const struct stat *st, const Options *options, char *out, size_t size);
void format_blocks(uintmax_t blocks, const Options *options, char *out, size_t size);
void format_time(const struct stat *st, const Options *options, char *out, size_t size);
struct timespec selected_time(const struct stat *st, const Options *options);
char format_suffix(mode_t mode);
uintmax_t block_unit_from_env(void);

#endif
