#include "options.h"

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

int
options_parse(int argc, char **argv, Options *options, int *first_operand)
{
	int option;

	*options = (Options){0};
	options->block_unit = 512;
	opterr = 0;
	optind = 1;
	while ((option = getopt(argc, argv, "AacdFfhiklnqRrSstuw")) != -1) {
		switch (option) {
		case 'A': options->almost_all = true; break;
		case 'a': options->all = true; break;
		case 'c': options->use_ctime = true; options->use_atime = false; break;
		case 'd': options->directory_as_file = true; options->recursive = false; break;
		case 'F': options->classify = true; break;
		case 'f': options->no_sort = true; break;
		case 'h': options->human = true; options->kilobytes = false; break;
		case 'i': options->inode = true; break;
		case 'k': options->kilobytes = true; options->human = false; break;
		case 'l': options->long_format = true; options->numeric_ids = false; break;
		case 'n': options->numeric_ids = true; options->long_format = true; break;
		case 'q': options->force_question = true; options->force_raw = false; break;
		case 'R': options->recursive = true; options->directory_as_file = false; break;
		case 'r': options->reverse = true; break;
		case 'S': options->sort_size = true; options->sort_time = false; break;
		case 's': options->show_blocks = true; break;
		case 't': options->sort_time = true; options->sort_size = false; break;
		case 'u': options->use_atime = true; options->use_ctime = false; break;
		case 'w': options->force_raw = true; options->force_question = false; break;
		case '?':
			if (optopt != 0)
				fprintf(stderr, "ls: invalid option -- '%c'\n", optopt);
			else
				fprintf(stderr, "ls: invalid option\n");
			return -1;
		default:
			return -1;
		}
	}
	*first_operand = optind;
	return 0;
}
