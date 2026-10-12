#include "listing.h"
#include "format.h"

#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A bounded recursion stack also prevents stack exhaustion on hostile trees. */
#define MAX_DEPTH 256

typedef struct {
    char *name;
    char *path;
    struct stat st;
    char *owner;
    char *group;
} Entry;

typedef struct {
    Entry *items;
    size_t count;
    size_t capacity;
} EntryList;

typedef struct Ancestor {
    dev_t device;
    ino_t inode;
    const struct Ancestor *parent;
} Ancestor;

typedef struct {
    int inode, blocks, links, owner, group, size;
} Widths;

static int had_error;
static bool printed_output;
static const Options *sort_options;

static void
fail_memory(void)
{
    fputs("ls: out of memory\n", stderr);
    exit(EXIT_FAILURE);
}

static void *
allocate(size_t size)
{
    void *result = malloc(size);
    if (!result) fail_memory();
    return result;
}

static char *
copy_string(const char *text)
{
    char *result = strdup(text);
    if (!result) fail_memory();
    return result;
}

static void
report_error(const char *path)
{
    int saved = errno;
    fprintf(stderr, "ls: %s: %s\n", path, strerror(saved));
    had_error = 1;
}

static char *
join_path(const char *directory, const char *name)
{
    size_t a = strlen(directory), b = strlen(name);
    bool slash = a > 0 && directory[a - 1] != '/';
    char *path;
    if (a > SIZE_MAX - 2 || b > SIZE_MAX - a - 2) fail_memory();
    path = allocate(a + b + 2);
    memcpy(path, directory, a);
    if (slash) path[a++] = '/';
    memcpy(path + a, name, b + 1);
    return path;
}

static char *
owner_name(uid_t uid, bool numeric)
{
    struct passwd *record;
    char number[64];
    if (!numeric && (record = getpwuid(uid)) != NULL)
        return copy_string(record->pw_name);
    snprintf(number, sizeof(number), "%ju", (uintmax_t)uid);
    return copy_string(number);
}

static char *
group_name(gid_t gid, bool numeric)
{
    struct group *record;
    char number[64];
    if (!numeric && (record = getgrgid(gid)) != NULL)
        return copy_string(record->gr_name);
    snprintf(number, sizeof(number), "%ju", (uintmax_t)gid);
    return copy_string(number);
}

static void
list_add(EntryList *list, const char *name, const char *path,
    const struct stat *st, const Options *options)
{
    Entry *entry;
    if (list->count == list->capacity) {
        size_t capacity;
        Entry *grown;
        if (list->capacity > SIZE_MAX / 2 / sizeof(*grown)) fail_memory();
        capacity = list->capacity ? list->capacity * 2 : 32;
        grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) fail_memory();
        list->items = grown;
        list->capacity = capacity;
    }
    entry = &list->items[list->count++];
    *entry = (Entry){0};
    entry->name = copy_string(name);
    entry->path = copy_string(path);
    entry->st = *st;
    if (options->long_format) {
        entry->owner = owner_name(st->st_uid, options->numeric_ids);
        entry->group = group_name(st->st_gid, options->numeric_ids);
    }
}

static void
list_free(EntryList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++) {
        free(list->items[i].name);
        free(list->items[i].path);
        free(list->items[i].owner);
        free(list->items[i].group);
    }
    free(list->items);
    *list = (EntryList){0};
}

static int
compare_entries(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    const Options *options = sort_options;
    int result = 0;
    if (options->sort_size) {
        result = (a->st.st_size < b->st.st_size) -
            (a->st.st_size > b->st.st_size);
    } else if (options->sort_time) {
        struct timespec at = selected_time(&a->st, options);
        struct timespec bt = selected_time(&b->st, options);
        result = (at.tv_sec < bt.tv_sec) - (at.tv_sec > bt.tv_sec);
        if (!result)
            result = (at.tv_nsec < bt.tv_nsec) - (at.tv_nsec > bt.tv_nsec);
    }
    if (!result) {
        int lexical = strcoll(a->name, b->name);
        if (!lexical) lexical = strcmp(a->name, b->name);
        result = (lexical > 0) - (lexical < 0);
    }
    return options->reverse ? -result : result;
}

static void
sort_entries(EntryList *list, const Options *options)
{
    if (!options->no_sort && list->count > 1) {
        sort_options = options;
        qsort(list->items, list->count, sizeof(*list->items), compare_entries);
    }
}

static bool
should_show(const char *name, const Options *options)
{
    if (name[0] != '.' || options->all) return true;
    return options->almost_all && strcmp(name, ".") && strcmp(name, "..");
}

static int
load_directory(const char *path, const Options *options, EntryList *list,
    struct stat *directory_stat)
{
    DIR *directory = opendir(path);
    if (!directory) {
        report_error(path);
        return -1;
    }
    if (fstat(dirfd(directory), directory_stat) != 0) {
        report_error(path);
        closedir(directory);
        return -1;
    }
    for (;;) {
        struct dirent *item;
        struct stat st;
        char *full_path;
        /* Other operations in this loop may change errno before readdir. */
        errno = 0;
        item = readdir(directory);
        if (!item) {
            if (errno) report_error(path);
            break;
        }
        if (!should_show(item->d_name, options)) continue;
        full_path = join_path(path, item->d_name);
        if (lstat(full_path, &st) == 0) {
            list_add(list, item->d_name, full_path, &st, options);
        } else {
            report_error(full_path);
        }
        free(full_path);
    }
    if (closedir(directory) != 0) report_error(path);
    sort_entries(list, options);
    return 0;
}

static int
number_width(uintmax_t value)
{
    int width = 1;
    while (value >= 10) {
        value /= 10;
        width++;
    }
    return width;
}

static void
increase_width(int *width, size_t length)
{
    if (length > INT_MAX) fail_memory();
    if (*width < (int)length) *width = (int)length;
}

static Widths
measure_entries(const EntryList *list, const Options *options)
{
    Widths widths = {0};
    size_t i;
    for (i = 0; i < list->count; i++) {
        const Entry *entry = &list->items[i];
        char text[128];
        increase_width(&widths.inode, number_width((uintmax_t)entry->st.st_ino));
        if (options->show_blocks) {
            format_blocks((uintmax_t)entry->st.st_blocks, options, text, sizeof(text));
            increase_width(&widths.blocks, strlen(text));
        }
        if (options->long_format) {
            increase_width(&widths.links, number_width((uintmax_t)entry->st.st_nlink));
            increase_width(&widths.owner, strlen(entry->owner));
            increase_width(&widths.group, strlen(entry->group));
            format_size(&entry->st, options, text, sizeof(text));
            increase_width(&widths.size, strlen(text));
        }
    }
    return widths;
}

static char *
link_target(const char *path)
{
    size_t capacity = 128;
    for (;;) {
        char *buffer = allocate(capacity + 1);
        ssize_t length = readlink(path, buffer, capacity);
        if (length < 0) {
            report_error(path);
            free(buffer);
            return NULL;
        }
        if ((size_t)length < capacity) {
            buffer[length] = '\0';
            return buffer;
        }
        free(buffer);
        if (capacity > (SIZE_MAX - 1) / 2) fail_memory();
        capacity *= 2;
    }
}

static void
print_entry(const Entry *entry, const Options *options, const Widths *widths)
{
    char text[128], timebuf[128], mode[11];
    char suffix = options->classify ? format_suffix(entry->st.st_mode) : '\0';
    if (options->inode)
        printf("%*ju ", widths->inode, (uintmax_t)entry->st.st_ino);
    if (options->show_blocks) {
        format_blocks((uintmax_t)entry->st.st_blocks, options, text, sizeof(text));
        printf("%*s ", widths->blocks, text);
    }
    if (options->long_format) {
        format_mode(entry->st.st_mode, mode);
        format_size(&entry->st, options, text, sizeof(text));
        format_time(&entry->st, options, timebuf, sizeof(timebuf));
        printf("%s %*ju %-*s %-*s %*s %s ", mode,
            widths->links, (uintmax_t)entry->st.st_nlink,
            widths->owner, entry->owner, widths->group, entry->group,
            widths->size, text, timebuf);
    }
    format_name(entry->name, options);
    if (suffix) putchar(suffix);
    if (options->long_format && S_ISLNK(entry->st.st_mode)) {
        char *target = link_target(entry->path);
        if (target) {
            fputs(" -> ", stdout);
            format_name(target, options);
            free(target);
        }
    }
    putchar('\n');
    printed_output = true;
}

static void
print_entries(const EntryList *list, const Options *options, bool directory)
{
    Widths widths = measure_entries(list, options);
    size_t i;
    if (directory && (options->long_format ||
        (options->show_blocks && isatty(STDOUT_FILENO)))) {
        uintmax_t total = 0;
        char text[128];
        for (i = 0; i < list->count; i++) {
            uintmax_t blocks = (uintmax_t)list->items[i].st.st_blocks;
            if (blocks > UINTMAX_MAX - total) {
                fputs("ls: block total overflow\n", stderr);
                had_error = 1;
                break;
            }
            total += blocks;
        }
        format_blocks(total, options, text, sizeof(text));
        printf("total %s\n", text);
        printed_output = true;
    }
    for (i = 0; i < list->count; i++) print_entry(&list->items[i], options, &widths);
}

static void
print_directory(const char *path, const Options *options, bool header,
    const Ancestor *parent, unsigned depth)
{
    EntryList list = {0};
    struct stat st;
    Ancestor current;
    const Ancestor *ancestor;
    size_t i;

    if (depth > MAX_DEPTH) {
        fprintf(stderr, "ls: %s: recursion depth exceeds %d\n", path, MAX_DEPTH);
        had_error = 1;
        return;
    }
    if (load_directory(path, options, &list, &st) != 0) return;
    for (ancestor = parent; ancestor; ancestor = ancestor->parent) {
        if (ancestor->device == st.st_dev && ancestor->inode == st.st_ino) {
            fprintf(stderr, "ls: %s: recursive directory cycle\n", path);
            had_error = 1;
            list_free(&list);
            return;
        }
    }
    current = (Ancestor){st.st_dev, st.st_ino, parent};
    if (header) {
        if (printed_output) putchar('\n');
        format_name(path, options);
        fputs(":\n", stdout);
        printed_output = true;
    }
    print_entries(&list, options, true);
    if (options->recursive) {
        for (i = 0; i < list.count; i++) {
            const Entry *entry = &list.items[i];
            if (S_ISDIR(entry->st.st_mode) && strcmp(entry->name, ".") &&
                strcmp(entry->name, "..")) {
                print_directory(entry->path, options, true, &current, depth + 1);
            }
        }
    }
    list_free(&list);
}

int
listing_run(int operand_count, char **operands, const Options *options)
{
    EntryList files = {0}, directories = {0};
    int i;
    size_t j;
    had_error = 0;
    printed_output = false;
    for (i = 0; i < operand_count; i++) {
        struct stat st;
        if (lstat(operands[i], &st) != 0) {
            report_error(operands[i]);
            continue;
        }
        /* A plain command-line link to a directory names its contents.
         * -d, -l/-n and -F inspect the link itself; recursive entries never follow links. */
        if (S_ISLNK(st.st_mode) && !options->directory_as_file &&
            !options->long_format && !options->classify) {
            struct stat target;
            if (stat(operands[i], &target) == 0 && S_ISDIR(target.st_mode)) st = target;
        }
        if (options->directory_as_file || !S_ISDIR(st.st_mode))
            list_add(&files, operands[i], operands[i], &st, options);
        else
            list_add(&directories, operands[i], operands[i], &st, options);
    }
    sort_entries(&files, options);
    sort_entries(&directories, options);
    print_entries(&files, options, false);
    for (j = 0; j < directories.count; j++) {
        print_directory(directories.items[j].path, options,
            operand_count > 1 || options->recursive, NULL, 0);
    }
    list_free(&files);
    list_free(&directories);
    return had_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
