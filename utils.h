#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Implement parser combinators using String_View

// Dynamic Array

#define da_reserve(da, additional)\
    do {\
        if (((da)->count + additional) > (da)->capacity) {\
            if ((da)->capacity == 0) {\
                (da)->capacity = 1;\
            }\
            while (((da)->count + additional) > (da)->capacity) {\
                (da)->capacity *= 2;\
            }\
            (da)->data = realloc((da)->data, (da)->capacity * sizeof(*(da)->data));\
        }\
    } while (0)

#define da_insert(da, index, value)\
    do {\
        da_reserve((da), 1);\
        memmove((da)->data + (index) + 1, (da)->data + (index),\
        ((da)->count++ - (index)) * sizeof(*(da)->data));\
        (da)->data[index] = (value);\
    } while (0)

#define da_remove(da, index)\
    do {\
        memmove((da)->data + (index), (da)->data + (index) + 1,\
        ((da)->count-- - (index)) * sizeof(*(da)->data));\
    } while (0)

#define da_push(da, value)\
    do {\
        da_reserve((da), 1);\
        (da)->data[(da)->count++] = (value);\
    } while (0)

#define da_append_raw(da, other, other_count)\
    do {\
        da_reserve((da), (other_count));\
        memcpy((da)->data + (da)->count, (other), (other_count) * sizeof(*(da)->data));\
        (da)->count += (other_count);\
    } while (0)

#define da_append(da, other) da_append_raw((da), (other)->data, (other)->count)

#define da_at(da, i) (da)->data[assert((da)->count > i), i]
#define da_first(da) (da)->data[assert((da)->count > 0), 0]
#define da_last(da) (da)->data[assert((da)->count > 0), (da)->count - 1]
#define da_pop(da) (da)->data[assert((da)->count > 0), --(da)->count]
#define da_foreach(x, da) for (typeof((da)->data[0]) *x = (da)->data; x < (da)->data + (da)->count; x++)

// String Builder and View

typedef struct {
    char* data;
    size_t count;
    size_t capacity;
} String_Builder;

typedef struct {
    const char* data;
    size_t count;
} String_View;

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)(sv).count, (sv).data

int sb_appendf(String_Builder *sb, const char *fmt, ...);
bool sb_read_file(String_Builder *sb, const char *path);

String_View sv_from_parts(const char *cstr, size_t count);
String_View sv_from_cstr(const char *cstr);

bool sv_eq(String_View a, String_View b);
bool sv_eq_cstr(String_View sv, char *cstr);

bool sv_starts_with(String_View sv, String_View start);
bool sv_ends_with(String_View sv, String_View end);

int sv_find_char(String_View sv, char pattern);
int sv_find_pred(String_View sv, int (*pattern)(int));
int sv_find_sv(String_View sv, String_View pattern);
int sv_find_cstr(String_View sv, char *pattern);

int sv_find_nth_char(String_View sv, char pattern, int nth);
int sv_find_nth_pred(String_View sv, int (*pattern)(int), int nth);
int sv_find_nth_sv(String_View sv, String_View pattern, int nth);
int sv_find_nth_cstr(String_View sv, char *pattern, int nth);

String_View sv_slice(String_View sv, size_t start, size_t end);
String_View sv_slice_start(String_View sv, size_t start);
String_View sv_slice_end(String_View sv, size_t end);

String_View sv_split_at(String_View *sv, size_t n);
String_View sv_rsplit_at(String_View *sv, size_t n);
String_View sv_split_delim(String_View *sv, char delim);
String_View sv_rsplit_delim(String_View *sv, char delim);

String_View sv_trim(String_View sv);
String_View sv_trim_start(String_View sv);
String_View sv_trim_end(String_View sv);

#ifdef UTILS_IMPLEMENTATION
int isnotspace(int c) {
    return c != ' ' && c != '\t' && c != '\n';
}

int sb_appendf(String_Builder *sb, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    da_reserve(sb, n + 1);
    char *dest = sb->data + sb->count;

    va_start(args, fmt);
    vsnprintf(dest, n+1, fmt, args);
    va_end(args);

    sb->count += n;

    return n;
}

bool sb_read_file(String_Builder *sb, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    int n = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    da_reserve(sb, n);
    fread(sb->data + sb->count, n, 1, fp);
    sb->count += n;

    return 0;
}

String_View sv_from_parts(const char *data, size_t count) {
    return (String_View){
        .data = data,
        .count = count,
    };
}

String_View sv_from_sb(String_Builder sb) {
    return sv_from_parts(sb.data, sb.count);
}

String_View sv_from_cstr(const char *cstr) {
    size_t len = 0;

    while (*(cstr + len) != '\0') len++;

    return sv_from_parts(cstr, len);
}

bool sv_eq(String_View a, String_View b) {
    if (a.count != b.count) return false;

    for (size_t i = 0; i < a.count; i++) {
        if (a.data[i] != b.data[i]) return false;
    }

    return true;
}

bool sv_eq_cstr(String_View sv, char *cstr) {
    for (size_t i = 0; i < sv.count; i++) {
        if (cstr[i] == '\0' || sv.data[i] != cstr[i]) return false;
    }

    return cstr[sv.count] == '\0';
}

bool sv_starts_with(String_View sv, String_View start) {
    return sv_eq(sv_slice_start(sv, start.count), start);
}

bool sv_ends_with(String_View sv, String_View end) {
    return sv_eq(sv_slice_end(sv, sv.count - end.count), end);
}

int sv_find_char(String_View sv, char pattern) {
    return sv_find_nth_char(sv, pattern, 1);
}

int sv_find_pred(String_View sv, int (*pattern)(int)) {
    return sv_find_nth_pred(sv, pattern, 1);
}

int sv_find_sv(String_View sv, String_View pattern) {
    return sv_find_nth_sv(sv, pattern, 1);
}

int sv_find_cstr(String_View sv, char *pattern) {
    return sv_find_nth_cstr(sv, pattern, 1);
}

int sv_find_nth_char(String_View sv, char pattern, int nth) {
    int count = 0;

    if (nth == 0) return -1;

    if (nth > 0) {
        for (size_t i = 0; i < sv.count; i++) {
            if (sv.data[i] == pattern) {
                count++;
                if (count == nth) return i;
            }
        }
    } else {
        for (int i = sv.count - 1; i >= 0; i--) {
            if (sv.data[i] == pattern) {
                count--;
                if (count == nth) return i;
            }
        }
    }

    return -1;
}

int sv_find_nth_pred(String_View sv, int (*pattern)(int), int nth) {
    int count = 0;

    if (nth == 0) return -1;

    if (nth > 0) {
        for (size_t i = 0; i < sv.count; i++) {
            if (pattern(sv.data[i])) {
                count++;
                if (count == nth) return i;
            }
        }
    } else {
        for (int i = sv.count - 1; i >= 0; i--) {
            if (pattern(sv.data[i])) {
                count--;
                if (count == nth) return i;
            }
        }
    }

    return -1;
}

int sv_find_nth_sv(String_View sv, String_View pattern, int nth) {
    int count = 0;

    if (nth == 0 || pattern.count == 0) return -1;

    if (nth > 0) {
        for (size_t i = 0; i < sv.count - pattern.count + 1; i++) {
            if (sv_starts_with(sv_slice_end(sv, i), pattern)) {
                count++;
                if (count == nth) return i;
            }
        }
    } else {
        for (int i = sv.count; i > (int)pattern.count - 1; i--) {
            if (sv_ends_with(sv_slice_start(sv, i), pattern)) {
                count--;
                if (count == nth) return i - pattern.count;
            }
        }
    }

    return -1;
}

int sv_find_nth_cstr(String_View sv, char* pattern, int nth) {
    return sv_find_nth_sv(sv, sv_from_cstr(pattern), nth);
}

String_View sv_slice(String_View sv, size_t start, size_t end) {
    return sv_from_parts(sv.data + start, end - start);
}

String_View sv_slice_start(String_View sv, size_t end) {
    return sv_slice(sv, 0, end);
}

String_View sv_slice_end(String_View sv, size_t start) {
    return sv_slice(sv, start, sv.count);
}

String_View sv_split_at(String_View *sv, size_t n) {
    String_View result = sv_from_parts(sv->data, n);
    sv->data += n;
    sv->count -= n;
    return result;
}

String_View sv_rsplit_at(String_View *sv, size_t n) {
    String_View result = sv_from_parts(sv->data + n, sv->count - n);
    sv->count = n;
    return result;
}

String_View sv_split_delim(String_View *sv, char delim) {
    int pos = sv_find_char(*sv, delim);
    String_View result = *sv;

    if (pos < 0) {
        *sv = (String_View){};
    } else {
        result = sv_slice_start(sv_split_at(sv, pos + 1), pos);
    }

    return result;
}

String_View sv_rsplit_delim(String_View *sv, char delim) {
    int pos = sv_find_nth_char(*sv, delim, -1);
    String_View result = *sv;

    if (pos < 0) {
        *sv = (String_View){};
    } else {
        result = sv_slice_end(sv_rsplit_at(sv, pos), 1);
    }

    return result;
}

String_View sv_trim(String_View sv) {
    return sv_trim_end(sv_trim_start(sv));
}

String_View sv_trim_start(String_View sv) {
    return sv_slice_end(sv, sv_find_pred(sv, isnotspace));
}

String_View sv_trim_end(String_View sv) {
    return sv_slice_start(sv, sv_find_nth_pred(sv, isnotspace, -1) + 1);
}
#endif
