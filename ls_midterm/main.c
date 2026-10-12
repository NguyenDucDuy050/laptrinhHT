#include "options.h"
#include "listing.h"
#include "format.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	Options options;
	int first_operand;
	int result;
	(void)setlocale(LC_ALL, "");
	if (options_parse(argc, argv, &options, &first_operand) != 0) return EXIT_FAILURE;
	if (geteuid() == 0) options.almost_all = true;
	if (!options.force_question && !options.force_raw)
		options.force_raw = !isatty(STDOUT_FILENO);
	options.block_unit = options.kilobytes ? 1024 :
	    (options.human ? 512 : block_unit_from_env());
	if (first_operand == argc) {
		char current[] = ".";
		result = listing_run(1, (char *[]){current}, &options);
	} else {
		result = listing_run(argc - first_operand, &argv[first_operand], &options);
	}
	if (fflush(stdout) == EOF || ferror(stdout)) {
		fputs("ls: cannot write standard output\n", stderr);
		result = EXIT_FAILURE;
	}
	return result;
}
