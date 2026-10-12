#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	bool all;
	bool almost_all;
	bool classify;
	bool directory_as_file;
	bool no_sort;
	bool show_blocks;
	bool human;
	bool inode;
	bool kilobytes;
	bool long_format;
	bool numeric_ids;
	bool recursive;
	bool reverse;
	bool sort_size;
	bool sort_time;
	bool use_ctime;
	bool use_atime;
	bool force_question;
	bool force_raw;
	uintmax_t block_unit;
} Options;

int options_parse(int argc, char **argv, Options *options, int *first_operand);

#endif
