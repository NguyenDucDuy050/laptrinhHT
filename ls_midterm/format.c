#include "format.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#ifdef __linux__
#include <sys/sysmacros.h>
#endif

static char
file_type(mode_t mode)
{
    if (S_ISREG(mode)) return '-';
    if (S_ISDIR(mode)) return 'd';
    if (S_ISLNK(mode)) return 'l';
    if (S_ISCHR(mode)) return 'c';
    if (S_ISBLK(mode)) return 'b';
    if (S_ISFIFO(mode)) return 'p';
#ifdef S_ISSOCK
    if (S_ISSOCK(mode)) return 's';
#endif
#ifdef S_ISWHT
    if (S_ISWHT(mode)) return 'w';
#endif
#ifdef S_ISARCH1
    if (S_ISARCH1(mode)) return 'a';
#endif
#ifdef S_ISARCH2
    if (S_ISARCH2(mode)) return 'A';
#endif
    return '?';
}

void
format_mode(mode_t mode, char out[11])
{
    static const mode_t bits[] = {
        S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
        S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH
    };
    static const char symbols[] = "rwxrwxrwx";
    size_t i;

    out[0] = file_type(mode);
    for (i = 0; i < 9; i++)
        out[i + 1] = (mode & bits[i]) ? symbols[i] : '-';
    if (mode & S_ISUID) out[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) out[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) out[9] = (mode & S_IXOTH) ? 't' : 'T';
    out[10] = '\0';
}

char
format_suffix(mode_t mode)
{
    if (S_ISDIR(mode)) return '/';
    if (S_ISLNK(mode)) return '@';
#ifdef S_ISSOCK
    if (S_ISSOCK(mode)) return '=';
#endif
    if (S_ISFIFO(mode)) return '|';
#ifdef S_ISWHT
    if (S_ISWHT(mode)) return '%';
#endif
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) return '*';
    return '\0';
}

void
format_name(const char *name, const Options *options)
{
    mbstate_t state = {0};
    size_t remaining = strlen(name);

    if (options->force_raw) {
        fputs(name, stdout);
        return;
    }
    /* Decode characters so printable UTF-8 names survive -q in a UTF-8 locale. */
    while (remaining > 0) {
        wchar_t character;
        size_t length = mbrtowc(&character, name, remaining, &state);
        if (length == (size_t)-1 || length == (size_t)-2) {
            putchar('?');
            length = 1;
            memset(&state, 0, sizeof(state));
        } else if (length == 0) {
            break;
        } else if (iswprint(character)) {
            fwrite(name, 1, length, stdout);
        } else {
            putchar('?');
        }
        name += length;
        remaining -= length;
    }
}

static void
human_size(long double bytes, char *out, size_t size)
{
    static const char units[] = "BKMGTPE";
    size_t unit = 0;
    while (bytes >= 1024.0L && unit < sizeof(units) - 2) {
        bytes /= 1024.0L;
        unit++;
    }
    if (unit > 0 && bytes < 9.95L)
        snprintf(out, size, "%.1Lf%c", bytes, units[unit]);
    else
        snprintf(out, size, "%.0Lf%c", bytes, units[unit]);
}

uintmax_t
block_unit_from_env(void)
{
    const char *value = getenv("BLOCKSIZE");
    char *end;
    uintmax_t unit, multiplier = 1;
    int exponent = 0;

    if (!value || !*value) return 512;
    if (*value == '-') goto invalid;
    errno = 0;
    unit = strtoumax(value, &end, 10);
    if (errno != 0) goto invalid;
    if (end == value) unit = 1;
    switch (*end) {
    case 'k': case 'K': exponent = 1; end++; break;
    case 'm': case 'M': exponent = 2; end++; break;
    case 'g': case 'G': exponent = 3; end++; break;
    case 't': case 'T': exponent = 4; end++; break;
    case 'b': case 'B': end++; break;
    case '\0': break;
    default: goto invalid;
    }
    if (exponent && (*end == 'b' || *end == 'B')) end++;
    if (*end || unit == 0) goto invalid;
    while (exponent-- > 0) multiplier *= 1024;
    if (unit > UINTMAX_MAX / multiplier) goto invalid;
    return unit * multiplier;
invalid:
    fprintf(stderr, "ls: invalid BLOCKSIZE; using 512 bytes\n");
    return 512;
}

void
format_blocks(uintmax_t blocks, const Options *options, char *out, size_t size)
{
    uintmax_t bytes;
    if (options->human) {
        human_size((long double)blocks * 512.0L, out, size);
        return;
    }
    if (blocks > UINTMAX_MAX / 512) {
        snprintf(out, size, "overflow");
        return;
    }
    bytes = blocks * 512;
    snprintf(out, size, "%ju", bytes / options->block_unit +
        (bytes % options->block_unit != 0));
}

void
format_size(const struct stat *st, const Options *options, char *out, size_t size)
{
    if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
        snprintf(out, size, "%ju, %ju", (uintmax_t)major(st->st_rdev),
            (uintmax_t)minor(st->st_rdev));
    } else if (options->human) {
        human_size((long double)st->st_size, out, size);
    } else {
        snprintf(out, size, "%jd", (intmax_t)st->st_size);
    }
}

struct timespec
selected_time(const struct stat *st, const Options *options)
{
#ifdef __APPLE__
    if (options->use_ctime) return st->st_ctimespec;
    if (options->use_atime) return st->st_atimespec;
    return st->st_mtimespec;
#else
    if (options->use_ctime) return st->st_ctim;
    if (options->use_atime) return st->st_atim;
    return st->st_mtim;
#endif
}

void
format_time(const struct stat *st, const Options *options, char *out, size_t size)
{
    struct tm local;
    time_t value = selected_time(st, options).tv_sec;
    if (!localtime_r(&value, &local) ||
        strftime(out, size, "%b %e %H:%M", &local) == 0)
        snprintf(out, size, "??? ?? ??:??");
}
