# Midterm Project: `ls(1)`

Student: Nguyen Duc Duy  
Student ID: 24IT050  
GitHub: https://github.com/NguyenDucDuy050/laptrinhHT.git

This project implements the required subset of the NetBSD `ls(1)` manual from scratch in portable C. It uses directory and metadata system calls rather than invoking the system `ls` command.

## Implemented options

`-A -a -c -d -F -f -h -i -k -l -n -q -R -r -S -s -t -u -w`

The option interactions described by the manual are implemented: the last of `-q/-w`, `-l/-n`, `-c/-u`, and `-R/-d` takes effect. Hidden entries, symbolic links, file type suffixes, recursive traversal, long format, inode/block counts, size/time sorting, reverse sorting, human-readable sizes, numeric IDs, `BLOCKSIZE`, `TZ`, and non-printable filename handling are supported.

## Build and run

```sh
make
./ls
./ls -laF .
./ls -lR /tmp
./ls -S -r file1 file2 directory
```

Run `make clean` to remove the executable and object files.

Run `make test` after building to execute the included smoke tests. The tests create a temporary directory and cover hidden entries, classification, long format, recursion, `-d`, symlinks, and missing operands.

## Error handling

The program reports filesystem errors on standard error, continues with other operands where possible, and exits with status `0` on success or a positive status when an error occurs. It avoids following symbolic links while inspecting directory entries by using `lstat(2)`.

## Files

- `main.c`: program entry point and root-user/default operand handling.
- `options.c/.h`: command-line parsing and option precedence.
- `listing.c/.h`: directory traversal, sorting, recursion, and output.
- `format.c/.h`: modes, owners/groups, sizes, timestamps, suffixes, and columns.
- `Makefile`: compiler and build rules.
- `.gitignore`: excludes the executable, object files, and core dumps.

## Submission checklist

1. Create a GitHub repository named `NguyenDucDuy_24IT050_midterm`.
2. Commit the source files, `Makefile`, `README.md`, and `.gitignore`.
3. Do not commit `ls`, `*.o`, or generated system files.
4. Add the repository URL to the final report before submitting it through the e-learning system.
