/**
 *                     _
 *   ___  __   __     | |__
 *  / __| \ \ / /     | '_ \
 *  \__ \  \ V /   _  | | | |
 *  |___/   \_/   (_) |_| |_|
 *
 *  sv.h - v1.0.0
 *
 *
 *  General notes:
 *  - StringView is a non-owning view of memory. It never allocates or frees.
 *  - The caller is responsible for ensuring the underlying memory remains valid
 *    for the lifetime of the view.
 *  - Bounds & OOR policy:
 *      * Strict (no checks): sv_at/sv_first/sv_last/sv_substr/sv_take/sv_drop, caller must keep indices/counts in range. OOR is undefined behavior (asserts in debug builds).
 *      * Safe (validated/clamped): find/rfind/_from/split/contains/parse are never UB due to positions. They return SV_NPOS/false on failure and may clamp pos as documented.
 *  - All parsing functions are strict:
 *      * Decimal only (no hex, octal, etc.)
 *      * No leading/trailing whitespace
 *      * No locale-dependent formats
 *      * Overflow or invalid input -> return false
 *  - Trimming uses isspace(), which is locale-dependent.
 *
 *  LICENSE:
 *   `sp.h` is licensed under the 3-Clause BSD license. Full license text is
 *   at the end of this file.
 **/


#ifndef SV_H_
#define SV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef SV_ASSERT
#include <assert.h>
#define SV_ASSERT(cond) assert((cond))
#endif

// If you do not need sv_print() and do not want to have stdio.h
// as a transient include, then you may disable it by defining
// SV_NO_PRINT before including sv.h
#ifndef SV_NO_PRINT
#include <stdio.h>
#define SV_HAS_PRINT 1
#else
#define SV_HAS_PRINT 0
#endif

// SVDEF are prepended to all function declarations, user can override it as
// they want. For example, for internal linkage and single-translation-unit
// use, it might be a good idea to define it as "static inline".
#ifndef SVDEF
#define SVDEF
#endif

#ifndef SV_NPOS
#define SV_NPOS SIZE_MAX
#endif

#ifndef __cplusplus
#define SV_NO_PARAMS void
#else
#define SV_NO_PARAMS
#endif

#if defined(__cplusplus) &&  __cplusplus >= 201703L && !defined(SV_IGNORE_NODISCARD)
#define SV_NODISCARD [[nodiscard]]
#else
#define SV_NODISCARD
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L  && defined(SV_ADD_STD_STRING_VIEW_CONVERSION)
#define SV_CONV_STD_SV
#include <string_view>
#endif

#ifdef __cplusplus
#define SV_NOEXCEPT noexcept
#if defined(__cpp_noexcept_function_type) || __cplusplus >= 201103L
#define SV_NOEXCEPT_IF(expr) noexcept(expr)
#else
#define SV_NOEXCEPT_IF(expr)
#endif
#else
#define SV_NOEXCEPT
#define SV_NOEXCEPT_IF(expr)
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    const char *begin;
    size_t      length;
} StringView;



//
// Creation
//

#ifndef __cplusplus
#define SV_LIT(s) ((StringView){ (const char *)(s), sizeof(s) - 1 })
#else
#define SV_LIT(s) (StringView{ (const char *)(s), sizeof(s) - 1 })
#endif

// Empty view (begin = NULL, length = 0)
SV_NODISCARD SVDEF StringView sv_empty(SV_NO_PARAMS) SV_NOEXCEPT;

// View from a null-terminated C string.
// Does not copy or own memory, lifetime of cstr must outlive the view
SV_NODISCARD SVDEF StringView sv_from_cstr(const char *cstr) SV_NOEXCEPT;

// View from pointer + length. No validation.
// Caller must ensure memory is valid for [begin, begin+length)
SV_NODISCARD SVDEF StringView sv_from_parts(const char *begin, size_t length) SV_NOEXCEPT;

//
// Access, inspection
//

// No bounds checks. Caller must ensure i < sv.length
SV_NODISCARD SVDEF char sv_at(StringView sv, size_t i) SV_NOEXCEPT;
// First character. Undefined if sv is empty
SV_NODISCARD SVDEF char sv_first(StringView sv) SV_NOEXCEPT;
// Last character. Undefined if sv is empty
SV_NODISCARD SVDEF char sv_last(StringView sv) SV_NOEXCEPT;

// Slice [pos, pos+count). No bounds checks
SV_NODISCARD SVDEF StringView sv_substr(StringView sv, size_t pos, size_t count) SV_NOEXCEPT;

// Equality of two views
SV_NODISCARD SVDEF bool sv_eq(StringView sv1, StringView sv2) SV_NOEXCEPT;
// Equality vs null-terminated cstr. Cstr must be NULL-terminated.
SV_NODISCARD SVDEF bool sv_eq_cstr(StringView sv, const char *cstr) SV_NOEXCEPT;
// Equality vs cstr of specified length. cstr must be non-NULL and at least 'length' bytes readable.
SV_NODISCARD SVDEF bool sv_eq_cstr_n(StringView sv, const char *cstr, size_t length) SV_NOEXCEPT;

// True if length == 0
SV_NODISCARD SVDEF bool sv_is_empty(StringView sv) SV_NOEXCEPT;

// Prefix/suffix checks. Return false if prefix/suffix is longer than sv
SV_NODISCARD SVDEF bool sv_starts_with(StringView sv, StringView prefix) SV_NOEXCEPT;
SV_NODISCARD SVDEF bool sv_ends_with(StringView sv, StringView suffix) SV_NOEXCEPT;



//
// Searching
//

// Find first occurrence of c. Returns SV_NPOS if not found
SV_NODISCARD SVDEF size_t sv_find_char(StringView sv, char c) SV_NOEXCEPT;

// Find last occurrence of c. Returns SV_NPOS if not found
SV_NODISCARD SVDEF size_t sv_rfind_char(StringView sv, char c) SV_NOEXCEPT;

// Find c at or after pos. pos is clamped to [0, length]. Returns SV_NPOS if not found
SV_NODISCARD SVDEF size_t sv_find_char_from(StringView sv, size_t pos, char c) SV_NOEXCEPT;

// Find c at or before pos. pos is clamped to [0, length-1] if non-empty, returns SV_NPOS if empty or not found
SV_NODISCARD SVDEF size_t sv_rfind_char_from(StringView sv, size_t pos, char c) SV_NOEXCEPT;

// Find first occurence of needle. Returns SV_NPOS if not found.
SV_NODISCARD SVDEF size_t sv_find_substr(StringView hay, StringView needle) SV_NOEXCEPT;

// Find last occurence of needle. Returns SV_NPOS if not found.
SV_NODISCARD SVDEF size_t sv_rfind_substr(StringView hay, StringView needle) SV_NOEXCEPT;

// Find first occurrence of needle starting from pos. pos is clamped to [0, length].
// Empty needle returns the clamped pos. Returns SV_NPOS if not found.
SV_NODISCARD SVDEF size_t sv_find_substr_from(StringView hay, size_t pos, StringView needle) SV_NOEXCEPT;

// Find last occurrence of needle not after pos. pos is clamped to [0, length].
// Empty needle returns the clamped pos. Returns SV_NPOS if not found.
SV_NODISCARD SVDEF size_t sv_rfind_substr_from(StringView hay, size_t pos, StringView needle) SV_NOEXCEPT;

// Cstr utility wrappers
SV_NODISCARD SVDEF size_t sv_find_substr_cstr(StringView hay, const char *needle) SV_NOEXCEPT;
SV_NODISCARD SVDEF size_t sv_rfind_substr_cstr(StringView hay, const char *needle) SV_NOEXCEPT;
SV_NODISCARD SVDEF size_t sv_find_substr_cstr_from(StringView hay, size_t pos, const char *needle) SV_NOEXCEPT;
SV_NODISCARD SVDEF size_t sv_rfind_substr_cstr_from(StringView hay, size_t pos, const char *needle) SV_NOEXCEPT;

// True if hay contains needle
SV_NODISCARD SVDEF bool sv_contains(StringView hay, StringView needle) SV_NOEXCEPT;
SV_NODISCARD SVDEF bool sv_contains_cstr(StringView hay, const char *needle) SV_NOEXCEPT;



//
// Trimming, splitting
//

// Trim leading isspace() chars (locale-dependent)
SV_NODISCARD SVDEF StringView sv_trim_left(StringView sv) SV_NOEXCEPT;
// Trim trailing isspace() chars (locale-dependent)
SV_NODISCARD SVDEF StringView sv_trim_right(StringView sv) SV_NOEXCEPT;
// Trim both ends
SV_NODISCARD SVDEF StringView sv_trim(StringView sv) SV_NOEXCEPT;

// Return first n chars. No bounds checks. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_take(StringView sv, size_t n) SV_NOEXCEPT;

// Drop first n chars and return the remainder. No bounds checks. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_drop(StringView sv, size_t n) SV_NOEXCEPT;

// Return last n characters. No bounds checks are performed. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_take_last(StringView sv, size_t n) SV_NOEXCEPT;

// Drop last n chars and return the remainder. No bounds checks. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_drop_last(StringView sv, size_t n) SV_NOEXCEPT;

// Return first n chars and advance sv to the remainder. No bounds checks. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_take_and_consume(StringView *sv, size_t n) SV_NOEXCEPT;

// Drop first n chars and return the remainder, set sv to the remainder. No bounds checks. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_drop_and_consume(StringView *sv, size_t n) SV_NOEXCEPT;

// Return last n chars and shrink sv to exclude them. No bounds checks. if n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_take_last_and_consume(StringView *sv, size_t n) SV_NOEXCEPT;

// Drop last n chars and return the remainder, set sv to the remainder. No bounds checks. If n > sv.length, behavior is undefined.
SV_NODISCARD SVDEF StringView sv_drop_last_and_consume(StringView *sv, size_t n) SV_NOEXCEPT;

// Split on first delimiter. If not found, before=sv, after=empty
SVDEF void sv_split_first(StringView  sv,
                          char        delim,
                          StringView *out_before_delim,
                          StringView *out_after_delim) SV_NOEXCEPT;

// Split on last delimiter. If not found, before=sv, after=empty
SVDEF void sv_split_last(StringView  sv,
                         char        delim,
                         StringView *out_before_delim,
                         StringView *out_after_delim) SV_NOEXCEPT;

// Split on first delimiter from or after pos. pos is clamped to [0, length]. If not found, before=sv, after=empty
SVDEF void sv_split_once_from(StringView  sv,
                              char        delim,
                              size_t      pos,
                              StringView *out_before_delim,
                              StringView *out_after_delim) SV_NOEXCEPT;


//
// Conversion
//

// Copy into buffer and NULL-terminate. Always writes '\0' if buffer_size > 0.
// Returns number of chars copied (excluding NULL).
SV_NODISCARD SVDEF size_t sv_to_cstr(StringView  sv,
                                     char       *buffer,
                                     size_t      buffer_size) SV_NOEXCEPT;

// Strict decimal parse (base-10 only). No whitespace. Optional sign.
// Returns false on invalid or overflow.
SV_NODISCARD SVDEF bool sv_to_uint64(StringView sv, uint64_t *out) SV_NOEXCEPT;
SV_NODISCARD SVDEF bool sv_to_int64(StringView sv, int64_t *out) SV_NOEXCEPT;
SV_NODISCARD SVDEF bool sv_to_long(StringView sv, long *out) SV_NOEXCEPT;

// Strict decimal floating-point parse. Format: [sign]digits[.digits][e[+-]digits].
// Uses . as decimal separator. No whitespace, no inf/nan.
// Returns false on invalid syntax, NaN, or overflow after exponent scaling.
// Note: If the integer part has more than ~20 decimal digits and no exponent is present,
//       excess high-order digits are ignored (the result reflects the leading digits only).
//       If you require overflow rejection in that case, normalize inputs to use an
//       exponent or pre-validate length.
SV_NODISCARD SVDEF bool sv_to_double(StringView sv, double *out) SV_NOEXCEPT;



//
// Utility
//

// FNV-1a 64-bit hash. Not cryptographically secure.
SV_NODISCARD SVDEF uint64_t sv_hash(StringView sv) SV_NOEXCEPT;

// Write raw bytes (not NULL-terminated) to FILE* with fwrite().
// Returns false on any error.
#if SV_HAS_PRINT
SV_NODISCARD SVDEF bool sv_print(StringView sv, FILE *f) SV_NOEXCEPT;
#endif


#ifdef __cplusplus
} // extern "C"
#endif


//
// C++ std::string_view compatibility
//

#ifdef SV_CONV_STD_SV
SV_NODISCARD SVDEF StringView       sv_from_std_sv(std::string_view sv) SV_NOEXCEPT;
// cpp standard does not mandate string view construction for ptr+length be noexcept, but some implementations do anyway.
SV_NODISCARD SVDEF std::string_view sv_to_std_sv(StringView sv) SV_NOEXCEPT_IF(noexcept(std::string_view{sv.begin, sv.length}));
#endif


#ifdef SV_IMPLEMENTATION

#include <ctype.h>
#include <limits.h>
#include <string.h>


// memrchr is a GNU extension, so we make our own
static inline void *
sv_memrchr_(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--) {
        if (*--p == (unsigned char)c) return (void *)p;
    }
    return NULL;
}



SVDEF StringView
sv_empty(SV_NO_PARAMS) SV_NOEXCEPT
{

#ifndef __cplusplus
    StringView result = {0};
#else
    StringView result = {};
#endif

    return result;
}

SVDEF StringView
sv_from_cstr(const char *cstr) SV_NOEXCEPT
{
    if (!cstr) return sv_empty();

    StringView sv = sv_empty();
    sv.begin  = cstr;
    sv.length = strlen(cstr);
    return sv;
}

SVDEF StringView
sv_from_parts(const char *begin, size_t length) SV_NOEXCEPT
{
    SV_ASSERT(begin != NULL || length == 0);

    StringView sv = sv_empty();
    sv.begin  = begin;
    sv.length = length;
    return sv;
}

SVDEF char
sv_at(StringView sv, size_t i) SV_NOEXCEPT
{
    SV_ASSERT(i < sv.length);
    return sv.begin[i];
}

SVDEF char
sv_first(StringView sv) SV_NOEXCEPT
{
    SV_ASSERT(sv.length > 0);
    return sv.begin[0];
}

SVDEF char
sv_last(StringView sv) SV_NOEXCEPT
{
    SV_ASSERT(sv.length > 0);
    return sv.begin[sv.length-1];
}

SVDEF StringView
sv_substr(StringView sv, size_t pos, size_t count) SV_NOEXCEPT
{
    SV_ASSERT(pos <= sv.length);
    SV_ASSERT(count <= sv.length - pos);

    StringView result = sv_empty();
    result.begin = sv.begin + pos;
    result.length = count;

    return result;
}

SVDEF bool
sv_eq(StringView sv1, StringView sv2) SV_NOEXCEPT
{
    if (sv1.length != sv2.length) return false;
    if (sv1.length == 0) return true;

    return memcmp(sv1.begin, sv2.begin, sv1.length) == 0;
}

SVDEF bool
sv_eq_cstr(StringView sv, const char *cstr) SV_NOEXCEPT
{
    if (!cstr) return false;

    size_t length = strlen(cstr);
    return sv_eq_cstr_n(sv, cstr, length);
}

SVDEF bool
sv_eq_cstr_n(StringView sv, const char *cstr, size_t length) SV_NOEXCEPT
{
    if (!cstr) return false;

    if (sv.length != length) return false;
    return sv.length == 0 || memcmp(sv.begin, cstr, length) == 0;
}

SVDEF bool
sv_is_empty(StringView sv) SV_NOEXCEPT
{
    return sv.length == 0;
}

SVDEF bool
sv_starts_with(StringView sv, StringView prefix) SV_NOEXCEPT
{
    if (prefix.length > sv.length) return 0;

    return memcmp(sv.begin, prefix.begin, prefix.length) == 0;
}

SVDEF bool
sv_ends_with(StringView sv, StringView suffix) SV_NOEXCEPT
{
    if (suffix.length > sv.length) return 0;

    const size_t offset = sv.length - suffix.length;
    return memcmp(sv.begin + offset, suffix.begin, suffix.length) == 0;
}

SVDEF size_t
sv_find_char(StringView sv, char c) SV_NOEXCEPT
{
    if (sv.length == 0) return SV_NPOS;

    const void *p = memchr(sv.begin, (unsigned char)c, sv.length);
    return p ? (size_t)((const char *)p - sv.begin) : SV_NPOS;
}

SVDEF size_t
sv_rfind_char(StringView sv, char c) SV_NOEXCEPT
{
    if (sv.length == 0) return SV_NPOS;

    const void *p = sv_memrchr_(sv.begin, (unsigned char)c, sv.length);
    return p ? (size_t)((const char *)p - sv.begin) : SV_NPOS;
}

SVDEF size_t
sv_find_char_from(StringView sv, size_t pos, char c) SV_NOEXCEPT
{
    if (sv.length == 0)  return SV_NPOS;
    if (pos > sv.length) pos = sv.length;

    const void *p = memchr(sv.begin + pos, (unsigned char)c, sv.length - pos);
    return p ? (size_t)((const char *)p - sv.begin) : SV_NPOS;
}

SVDEF size_t
sv_rfind_char_from(StringView sv, size_t pos, char c) SV_NOEXCEPT
{
    if (sv.length == 0)   return SV_NPOS;
    if (pos >= sv.length) pos = sv.length-1;

    const void *p = sv_memrchr_(sv.begin, (unsigned char)c, pos+1);
    return p ? (size_t)((const char *)p - sv.begin) : SV_NPOS;
}

SVDEF size_t
sv_find_substr(StringView hay, StringView needle) SV_NOEXCEPT
{
    if (needle.length == 0)         return 0;
    if (needle.length > hay.length) return SV_NPOS;

    const unsigned char first_char = (unsigned char)needle.begin[0];
    const size_t        last_start = hay.length - needle.length;

    for (size_t i = 0; i <= last_start;) {
        const void *p = memchr(hay.begin + i, first_char, (last_start - i) + 1);
        if (!p) return SV_NPOS;

        const size_t offset = (size_t)((const char *)p - hay.begin);
        if (memcmp(hay.begin + offset, needle.begin, needle.length) == 0) return offset;

        i = offset + 1;
    }
    return SV_NPOS;
}


SVDEF size_t
sv_rfind_substr(StringView hay, StringView needle) SV_NOEXCEPT
{
    if (needle.length == 0)         return hay.length;
    if (needle.length > hay.length) return SV_NPOS;

    const unsigned char first_char = (unsigned char)needle.begin[0];
    const size_t        last_start = hay.length - needle.length;

    for (size_t i = last_start + 1; i != 0;) {
        const void *p = sv_memrchr_(hay.begin, first_char, i);
        if (!p) return SV_NPOS;

        const size_t offset = (size_t)((const char *)p - hay.begin);
        if (memcmp(hay.begin + offset, needle.begin, needle.length) == 0)
            return offset;

        i = offset;
    }

    return SV_NPOS;
}

SVDEF size_t
sv_find_substr_from(StringView hay, size_t pos, StringView needle) SV_NOEXCEPT
{
    if (pos > hay.length)                 pos = hay.length;
    if (needle.length == 0)               return pos;
    if (needle.length > hay.length - pos) return SV_NPOS;

    const unsigned char first_char = (unsigned char)needle.begin[0];
    const size_t        last_start = hay.length - needle.length;

    for (size_t i = pos; i <= last_start;) {
        const void *p = memchr(hay.begin + i, first_char, (last_start - i) + 1);
        if (!p) return SV_NPOS;

        size_t offset = (size_t)((const char *)p - hay.begin);
        if (memcmp(hay.begin + offset, needle.begin, needle.length) == 0) {
            return offset;
        }
        i = offset + 1;
    }

    return SV_NPOS;
}

SVDEF size_t
sv_rfind_substr_from(StringView hay, size_t pos, StringView needle) SV_NOEXCEPT
{
    if (pos > hay.length)           pos = hay.length;
    if (needle.length == 0)         return pos;
    if (needle.length > hay.length) return SV_NPOS;
    if (pos + 1 < needle.length)    return SV_NPOS;

    size_t last_start = hay.length - needle.length;
    if (pos > last_start) pos = last_start;

    const unsigned char first_char = (unsigned char)needle.begin[0];

    for (size_t i = pos + 1; i != 0; ) {
        const void *p = sv_memrchr_(hay.begin, first_char, i);
        if (!p) return SV_NPOS;

        size_t offset = (size_t)((const char *)p - hay.begin);
        if (memcmp(hay.begin + offset, needle.begin, needle.length) == 0)
            return offset;

        i = offset;
    }

    return SV_NPOS;
}

SVDEF size_t
sv_find_substr_cstr(StringView hay, const char *needle) SV_NOEXCEPT
{
    StringView needle_sv = sv_from_cstr(needle);
    return sv_find_substr(hay, needle_sv);
}


SVDEF size_t
sv_rfind_substr_cstr(StringView hay, const char *needle) SV_NOEXCEPT
{
    StringView needle_sv = sv_from_cstr(needle);
    return sv_rfind_substr(hay, needle_sv);
}

SVDEF size_t
sv_find_substr_cstr_from(StringView hay, size_t pos, const char *needle) SV_NOEXCEPT
{
    StringView needle_sv = sv_from_cstr(needle);
    return sv_find_substr_from(hay, pos, needle_sv);
}

SVDEF size_t
sv_rfind_substr_cstr_from(StringView hay, size_t pos, const char *needle) SV_NOEXCEPT
{
    StringView needle_sv = sv_from_cstr(needle);
    return sv_rfind_substr_from(hay, pos, needle_sv);
}

SVDEF bool
sv_contains(StringView hay, StringView needle) SV_NOEXCEPT
{
    return sv_find_substr(hay, needle) != SV_NPOS;
}

SVDEF bool
sv_contains_cstr(StringView hay, const char *needle) SV_NOEXCEPT
{
    return sv_find_substr_cstr(hay, needle) != SV_NPOS;
}

SVDEF StringView
sv_trim_left(StringView sv) SV_NOEXCEPT
{
    size_t i = 0;
    while (i < sv.length && isspace((unsigned char)sv.begin[i])) i += 1;

    return sv_from_parts(sv.begin + i, sv.length - i);
}

SVDEF StringView
sv_trim_right(StringView sv) SV_NOEXCEPT
{
    size_t i = 0;
    while (i < sv.length && isspace((unsigned char)sv.begin[sv.length - i - 1])) i += 1;

    return sv_from_parts(sv.begin, sv.length - i);
}

SVDEF StringView
sv_trim(StringView sv) SV_NOEXCEPT
{
    return sv_trim_right(sv_trim_left(sv));
}

SVDEF StringView
sv_take(StringView sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv.length);
    return sv_substr(sv, 0, n);
}

SVDEF StringView
sv_drop(StringView sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv.length);
    return sv_substr(sv, n, sv.length-n);
}

SVDEF StringView
sv_take_last(StringView sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv.length);
    return sv_substr(sv, sv.length-n, n);
}

SVDEF StringView
sv_drop_last(StringView sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv.length);
    return sv_substr(sv, 0, sv.length-n);
}

SVDEF StringView
sv_take_and_consume(StringView *sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv->length);

    StringView out = sv_empty();
    out.begin = sv->begin;
    out.length = n;
    sv->begin  += n;
    sv->length -= n;
    return out;
}

SVDEF StringView
sv_drop_and_consume(StringView *sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv->length);

    sv->begin  += n;
    sv->length -= n;
    return *sv;
}

SVDEF StringView
sv_take_last_and_consume(StringView *sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv->length);

    StringView out = sv_empty();
    out.begin = sv->begin + (sv->length - n);
    out.length = n;
    sv->length -= n;
    return out;
}

SVDEF StringView
sv_drop_last_and_consume(StringView *sv, size_t n) SV_NOEXCEPT
{
    SV_ASSERT(n <= sv->length);

    sv->length -= n;
    return *sv;
}


SVDEF void
sv_split_first(StringView  sv,
               char        delim,
               StringView *out_before_delim,
               StringView *out_after_delim) SV_NOEXCEPT
{
    const void *p = (sv.length ? memchr(sv.begin, (unsigned char)delim, sv.length) : NULL);
    if (!p) {
        if (out_before_delim) *out_before_delim = sv;
        if (out_after_delim)  *out_after_delim  = sv_empty();
        return;
    }

    size_t idx = (size_t)((const char *)p - sv.begin);
    if (out_before_delim) *out_before_delim = sv_substr(sv, 0, idx);
    if (out_after_delim)  *out_after_delim  = sv_substr(sv, idx+1, sv.length-idx-1);
}

SVDEF void
sv_split_last(StringView  sv,
              char        delim,
              StringView *out_before_delim,
              StringView *out_after_delim) SV_NOEXCEPT
{
    const void *p = (sv.length ? sv_memrchr_(sv.begin, (unsigned char)delim, sv.length) : NULL);
    if (!p) {
        if (out_before_delim) *out_before_delim = sv;
        if (out_after_delim)  *out_after_delim  = sv_empty();
        return;
    }

    size_t idx = (size_t)((const char *)p - sv.begin);
    if (out_before_delim) *out_before_delim = sv_substr(sv, 0, idx);
    if (out_after_delim)  *out_after_delim  = sv_substr(sv, idx+1, sv.length-idx-1);
}

SVDEF void
sv_split_once_from(StringView  sv,
                   char        delim,
                   size_t      pos,
                   StringView *out_before_delim,
                   StringView *out_after_delim) SV_NOEXCEPT
{
    if (pos > sv.length) pos = sv.length;

    const void *p = (sv.length ? memchr(sv.begin + pos, (unsigned char)delim, sv.length - pos) : NULL);
    if (!p) {
        if (out_before_delim) *out_before_delim = sv;
        if (out_after_delim)  *out_after_delim  = sv_empty();
        return;
    }

    size_t idx = (size_t)((const char *)p - sv.begin);
    if (out_before_delim) *out_before_delim = sv_substr(sv, 0, idx);
    if (out_after_delim)  *out_after_delim  = sv_substr(sv, idx+1, sv.length-idx-1);
}

SVDEF size_t
sv_to_cstr(StringView  sv,           // Adds null-terminator, you should
           char       *buffer,       // allocate .length+1 to avoid truncation
           size_t      buffer_size)  SV_NOEXCEPT
{
    if (buffer_size == 0) {
        return 0;
    }

    size_t n = sv.length;
    if (n > buffer_size - 1) {
        n = buffer_size - 1;
    }

    if (n > 0) {
        memcpy(buffer, sv.begin, n);
    }
    buffer[n] = '\0';

    return n;
}

// Returns true on success, false on invalid/overflow.
// Accepts optional leading '+' for u64.
SVDEF bool
sv_to_uint64(StringView sv, uint64_t *out) SV_NOEXCEPT
{
    if (!out || sv.length == 0) return false;

    const char *p = sv.begin;
    size_t      n = sv.length;

    // Optional '+'
    if (*p == '+') { p++; if (--n == 0) return false; }

    uint64_t acc = 0;
    bool     got_digit = false;

    while (n--) {
        unsigned char c = (unsigned char)*p++;
        if (c < '0' || c > '9') return false;
        uint32_t d = (uint32_t)(c - '0');

        // acc*10 + d <= UINT64_MAX  =>  acc <= UINT64_MAX/10
        if (acc > UINT64_MAX / 10ULL ||
            (acc == UINT64_MAX / 10ULL && d > (UINT64_MAX % 10ULL))) {
            return false; // overflow
        }
        acc = acc * 10ULL + (uint64_t)d;
        got_digit = true;
    }
    if (!got_digit) return false;

    *out = acc;
    return true;
}

// Returns true on success, false on invalid/overflow.
// Accepts leading '+' or '-'.
SVDEF bool
sv_to_int64(StringView sv, int64_t *out) SV_NOEXCEPT
{
    if (!out || sv.length == 0) return false;

    const char *p = sv.begin;
    size_t      n = sv.length;

    bool neg = false;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++; if (--n == 0) return false;
    }

    // We parse as unsigned then range-check against the signed limits.
    uint64_t acc = 0;
    const uint64_t pos_limit = (uint64_t)INT64_MAX;          //  9223372036854775807
    const uint64_t neg_limit = (uint64_t)INT64_MAX + 1ULL;   //  9223372036854775808
    const uint64_t limit     = neg ? neg_limit : pos_limit;

    bool got_digit = false;
    while (n--) {
        unsigned char c = (unsigned char)*p++;
        if (c < '0' || c > '9') return false;
        uint32_t d = (uint32_t)(c - '0');

        if (acc > limit / 10ULL ||
            (acc == limit / 10ULL && (uint64_t)d > (limit % 10ULL))) {
            return false; // overflow
        }
        acc       = acc * 10ULL + (uint64_t)d;
        got_digit = true;
    }
    if (!got_digit) return false;

    if (neg) {
        if (acc == neg_limit) { *out = INT64_MIN; return true; }
        *out = -(int64_t)acc;
    } else {
        *out = (int64_t)acc;
    }
    return true;
}

SVDEF bool
sv_to_long(StringView sv, long *out) SV_NOEXCEPT
{
    if (!out) return false;
#if LONG_MAX == 2147483647L  // 32-bit long
    int64_t tmp;
    if (!sv_to_int64(sv, &tmp)) return false;
    if (tmp < LONG_MIN || tmp > LONG_MAX) return false;
    *out = (long)tmp;
    return true;
#else                         // 64-bit long
    int64_t tmp;
    if (!sv_to_int64(sv, &tmp)) return false;
    *out = (long)tmp;
    return true;
#endif
}

SVDEF bool
sv_to_double(StringView sv, double *out) SV_NOEXCEPT
{
    if (!out || sv.length == 0) return false;

    const char *p = sv.begin;
    size_t      n = sv.length;

    // Sign
    bool neg = false;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p += 1;
        if (--n == 0) return false;
    }

    // Integer part
    unsigned long long int_part = 0;
    bool               have_int = false;
    while (n && *p >= '0' && *p <= '9') {
        unsigned d = (unsigned)(*p - '0');
        if (int_part <= 1844674407370955161ULL) { // floor(ULLONG_MAX/10)
            int_part = int_part * 10ULL + (unsigned long long)d;
        }
        p += 1;
        n -= 1;
        have_int = true;
    }

    // Fractional part
    unsigned long long frac_acc    = 0; // exact up to 19 digits
    int                frac_digits = 0;
    bool               have_frac   = false;

    // track tail contribution for digits beyond 19
    double frac_tail = 0.0;
    double tail_scale = 1e-20; // weight of the 20th fractional digit

    if (n && *p == '.') {
        p += 1;
        n -= 1;
        while (n && *p >= '0' && *p <= '9') {
            unsigned d = (unsigned)(*p - '0');
            if (frac_digits < 19) {
                frac_acc = frac_acc * 10ULL + (unsigned long long)d;
            } else {
                // Add contribution of extra digits as we parse them
                frac_tail  += (double)d * tail_scale;
                tail_scale *= 0.1; // next digit is 10x smaller
            }
            frac_digits += 1;
            p += 1;
            n -= 1;
            have_frac = true;
        }
        if (!have_frac) return false; // '.' must be followed by at least one digit
    }

    if (!have_int && !have_frac) return false;

    // Exponent
    bool have_exp = false;
    bool exp_neg  = false;
    int  exp_val  = 0;
    if (n && (*p == 'e' || *p == 'E')) {
        have_exp = true;
        p += 1;
        n -= 1;
        if (!n) return false;
        if (*p == '+' || *p == '-') {
            exp_neg = (*p == '-');
            p += 1;
            if (--n == 0) return false;
        }
        bool got_exp = false;
        while (n && *p >= '0' && *p <= '9') {
            int d = (int)(*p - '0');
            if (exp_val < 10000) exp_val = exp_val * 10 + d; // cap growth
            p += 1;
            n -= 1;
            got_exp = true;
        }
        if (!got_exp) return false;
    }

    // Must consume everything
    if (n != 0) return false;

    // Build base value = int_part + frac_acc / 10^k (k up to 19)
    static const double k_pow10[20] = {
        1.0,
        1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
        1e10,1e11,1e12,1e13,1e14,1e15,1e16,1e17,1e18,1e19
    };

    double value      = (double)int_part;
    if (frac_digits > 0) {
        int    used = (frac_digits <= 19) ? frac_digits : 19;
        double frac = (double)frac_acc / k_pow10[used];
        value += frac + frac_tail;
    }

    // Apply exponent via chunked scaling (no pow)
    long exp_total = have_exp ? (exp_neg ? -exp_val : exp_val) : 0;

    if (value == 0.0) {
        *out = neg ? -0.0 : 0.0;
        return true;
    }

    if (exp_total >  308)  return false; // overflow for double
    if (exp_total < -400) {  // strong underflow
        *out = neg ? -0.0 : 0.0;
        return true;
    }

    static const double p10[9]     = { 1e1,  1e2,  1e4,   1e8,  1e16,  1e32,  1e64,  1e128,  1e256 };
    static const double p10_inv[9] = { 1e-1, 1e-2, 1e-4,  1e-8, 1e-16, 1e-32, 1e-64, 1e-128, 1e-256 };
    static const int    p10_exp[9] = { 1,    2,    4,     8,    16,    32,    64,    128,    256 };

    #define SV_FINITE(x) (((x) - (x)) == 0.0)

    double scaled = value;
    long   e      = exp_total;

    if (e > 0) {
        for (int i = 8; i >= 0; --i) {
            if (e >= p10_exp[i]) {
                scaled *= p10[i];
                if (!SV_FINITE(scaled)) return false; // overflow to inf/NaN
                e -= p10_exp[i];
            }
        }
    } else if (e < 0) {
        e = -e;
        for (int i = 8; i >= 0; --i) {
            if (e >= p10_exp[i]) {
                scaled *= p10_inv[i]; // multiply by reciprocal to avoid division
                e -= p10_exp[i];
            }
        }
    }

    *out = neg ? -scaled : scaled;
    return true;

    #undef SV_FINITE
}

// FNV-1a: http://www.isthe.com/chongo/tech/comp/fnv/
SVDEF uint64_t
sv_hash(StringView sv) SV_NOEXCEPT
{
    uint64_t hash = 1469598103934665603ULL; // offset basis
    for (size_t i = 0; i < sv.length; ++i) {
        hash ^= (unsigned char)sv.begin[i];
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

#if SV_HAS_PRINT
SVDEF bool
sv_print(StringView sv, FILE *f) SV_NOEXCEPT
{
    if (!f) return false;

    const char *p = sv.begin;
    size_t      n = sv.length;

    bool result = true;
    // fwrite() should usually write it all, but loop just in case of partial writes.
    while (n) {
        size_t w = fwrite(p, 1, n, f);
        if (w == 0) {
            // error or stream not writable
            result = false;
            break;
        }
        p += w;
        n -= w;
    }

    return result;
}
#endif // SV_HAS_PRINT


#ifdef SV_CONV_STD_SV

SVDEF StringView
sv_from_std_sv(std::string_view sv) SV_NOEXCEPT
{
    return StringView{sv.data(), sv.length()};
}

SVDEF std::string_view
sv_to_std_sv(StringView sv) SV_NOEXCEPT_IF(noexcept(std::string_view{sv.begin, sv.length}))
{
    return std::string_view(sv.begin, sv.length);
}

#endif // SV_CONV_STD_SV

#endif // SV_IMPLEMENTATION

#endif // SV_H_

/**
 * BSD-3-CLAUSE LICENSE
 *
 * Copyright 2025 rsore
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
