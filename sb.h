/**
 *        _           _
 *  ___  | |__       | |__
 * / __| | '_ \      | '_ \
 * \__ \ | |_) |  _  | | | |
 * |___/ |_.__/  (_) |_| |_|
 *
 *  sb.h - v1.0.0
 *
 *  This file is placed in the public domain.
 *  See end of file for license details.
 *
 **/


#ifndef SB_H_
#define SB_H_

#include <stddef.h>

#ifndef SBDEF
#define SBDEF
#endif

#ifndef SB_ASSERT
#include <assert.h>
#define SB_ASSERT(cond) assert((cond))
#endif

#if !defined(SB_REALLOC) && !defined(SB_FREE)
#include <stdlib.h>
#endif

#ifndef SB_REALLOC
#define SB_REALLOC(ptr, new_size) realloc((ptr), (new_size))
#endif

#ifndef SB_FREE
#define SB_FREE(ptr) free((ptr))
#endif


#ifndef SB_START_SIZE
#define SB_START_SIZE 64u
#endif

#ifndef SB_EXP_GROWTH_FACTOR
#define SB_EXP_GROWTH_FACTOR 2 // Exponential growth factor before threshold
#endif

#ifndef SB_LIN_THRESHOLD
#define SB_LIN_THRESHOLD (1u * 1024u * 1024u) // 1 MB before switching from growth factor to linear growth
#endif


#ifndef SB_LIN_GROWTH_FACTOR
#define SB_LIN_GROWTH_FACTOR (256u * 1024u) // 256 KB after threshold
#endif



#if defined(__cplusplus) &&  __cplusplus >= 201703L && !defined(SB_IGNORE_NODISCARD)
#define SB_NODISCARD [[nodiscard]]
#else
#define SB_NODISCARD
#endif

#ifndef __cplusplus
#define SB_NO_PARAMS void
#else
#define SB_NO_PARAMS
#endif

#ifdef __cplusplus
#define SB_NOEXCEPT noexcept
#else
#define SB_NOEXCEPT
#endif


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    char  *buffer;
    size_t capacity;
    size_t size;
} StringBuilder;



#ifdef __cplusplus
    #define SB_INIT {};
#else
    #define SB_INIT {0};
#endif

// Initialize an empty string. Equivelant to SB_INIT.
SB_NODISCARD SBDEF StringBuilder sb_init(SB_NO_PARAMS) SB_NOEXCEPT;

// Free builder buffer and reset
SBDEF void sb_free(StringBuilder *sb) SB_NOEXCEPT;

// Reset builder without deallocating
SBDEF void sb_reset(StringBuilder *sb) SB_NOEXCEPT;

SBDEF void sb_reserve(StringBuilder *sb, size_t new_cap) SB_NOEXCEPT;

// Append one sized string to builder
SBDEF void sb_append_one_n(StringBuilder *sb, const char *str, size_t len) SB_NOEXCEPT;
// Append one NULL-terminated string to builder
SBDEF void sb_append_one(StringBuilder *sb, const char *str) SB_NOEXCEPT;

// Append several NULL-terminated strings to builder.
#define sb_append(sb_ptr, ...) sb_append_((sb_ptr), __VA_ARGS__, NULL)
SBDEF void sb_append_(StringBuilder *sb, const char *new_data1, ...) SB_NOEXCEPT; // NOTE: Use sb_append() without underscore suffix

// Append one character to builder
SBDEF void sb_append_char(StringBuilder *sb, char c) SB_NOEXCEPT;

// Append one string builder's content to another's. 'app' is appended to 'sb'.
SBDEF void sb_append_sb(StringBuilder *sb, StringBuilder *app) SB_NOEXCEPT;

// Append a formatted string to builder. Formatting follows sprintf semantics.
SBDEF void sb_appendf(StringBuilder *sb, const char *fmt, ...) SB_NOEXCEPT;

// Allocate and return a null-terminated string of the contents of the builder. Caller must free with SB_FREE()
SB_NODISCARD SBDEF char *sb_to_cstr(StringBuilder *sb) SB_NOEXCEPT;

#ifdef __cplusplus
} // extern "C"
#endif



#ifdef SB_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

SBDEF StringBuilder
sb_init(SB_NO_PARAMS) SB_NOEXCEPT
{
    StringBuilder result = SB_INIT;

    return result;
}

SBDEF void
sb_free(StringBuilder *sb) SB_NOEXCEPT
{
    SB_FREE(sb->buffer);
    sb->buffer   = NULL;
    sb->capacity = 0;
    sb->size     = 0;
}

SBDEF void
sb_reset(StringBuilder *sb) SB_NOEXCEPT
{
    sb->size = 0;
}

static inline void
sb_grow_to_fit_(StringBuilder *sb, size_t n) SB_NOEXCEPT
{
    if (n <= sb->capacity) return;

    // Exponential growth until threshold
    size_t new_cap = sb->capacity ? sb->capacity : SB_START_SIZE;
    while (new_cap < n && new_cap < SB_LIN_THRESHOLD) {
        size_t pot = new_cap * SB_EXP_GROWTH_FACTOR;
        if (pot < new_cap) new_cap = n; // Overflow fallback
        new_cap = pot;
    }

    // Linear growth after threshold
    while (new_cap < n) {
        size_t pot = new_cap + SB_LIN_GROWTH_FACTOR;
        if (pot < new_cap) new_cap = n; // Overflow fallback
        new_cap = pot;
    }

    void *new_buffer = SB_REALLOC(sb->buffer, new_cap);
    SB_ASSERT(new_buffer != NULL);

    sb->buffer = (char *)new_buffer;
    sb->capacity = new_cap;
}

SBDEF void
sb_reserve(StringBuilder *sb, size_t new_cap) SB_NOEXCEPT
{
    sb_grow_to_fit_(sb, new_cap);
}

SBDEF void
sb_append_one_n(StringBuilder *sb, const char *str, size_t len) SB_NOEXCEPT
{
    sb_grow_to_fit_(sb, sb->size + len);

    memcpy(sb->buffer+sb->size, str, len);
    sb->size += len;
}

SBDEF void
sb_append_one(StringBuilder *sb, const char *str) SB_NOEXCEPT
{
    size_t len = strlen(str);
    sb_append_one_n(sb, str, len);
}

SBDEF void
sb_append_(StringBuilder *sb, const char *new_data1, ...) SB_NOEXCEPT
{
    va_list args;
    va_start(args, new_data1);

    const char *data = new_data1;
    while (data != NULL) {
        sb_append_one(sb, data);
        data = va_arg(args, const char *);
    }

    va_end(args);
}

SBDEF void
sb_append_char(StringBuilder *sb, char c) SB_NOEXCEPT
{
    sb_grow_to_fit_(sb, sb->size + 1);
    sb->buffer[sb->size++] = c;
}

SBDEF void
sb_append_sb(StringBuilder *sb, StringBuilder *app) SB_NOEXCEPT
{
    sb_append_one_n(sb, app->buffer, app->size);
}

SBDEF void
sb_appendf(StringBuilder *sb, const char *fmt, ...) SB_NOEXCEPT
{
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    size_t len_z = len + 1;

    sb_grow_to_fit_(sb, sb->size + len_z);

    va_start(args, fmt);
    vsnprintf(sb->buffer + sb->size, len_z, fmt, args);
    va_end(args);
    sb->size += len;
}

SBDEF char *
sb_to_cstr(StringBuilder *sb) SB_NOEXCEPT
{
    if (!sb->buffer || sb->size == 0) return NULL;

    char *buf = (char *)SB_REALLOC(NULL, sb->size + 1);
    if (!buf) return NULL;

    memcpy(buf, sb->buffer, sb->size);
    buf[sb->size] = '\0';

    return buf;
}

#endif // SB_IMPLEMENTATION

#endif // SB_H_
