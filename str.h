/**
 *        _                _
 *  ___  | |_   _ __      | |__
 * / __| | __| | '__|     | '_ \
 * \__ \ | |_  | |     _  | | | |
 * |___/  \__| |_|    (_) |_| |_|
 *
 *  str.h - v2.0.0
 *
 *
 *  OVERVIEW
 *  --------
 *  String is a growable mutable string.
 *  Invariants
 *    - buffer[size] is always '\0' when buffer != NULL
 *    - capacity is total bytes including the trailing NUL
 *    - size is number of content bytes, excludes NUL
 *
 *  Init and lifetime
 *    - str_init makes an empty string. Best effort allocates 1 byte for ""
 *    - str_clear keeps allocation and sets size to 0
 *    - str_free frees buffer and zeros the struct
 *    - str_move moves ownership and re-inits the source
 *
 *  Appending
 *    - str_append_one appends a NUL terminated cstr
 *    - str_append appends many cstr in one call
 *    - str_append_one_n appends an arbitrary buffer of length n
 *    - str_append_char and str_append_repeat for single char and runs
 *    - str_append_str appends another String. Self append is supported
 *
 *  Capacity
 *    - str_reserve(new_len) ensures room for new_len content bytes
 *      the trailing NUL is handled internally
 *    - str_shrink_to_fit sets capacity to size + 1
 *
 *  Ownership helpers
 *    - str_strdup returns a new copy. caller must STR_FREE
 *    - str_release returns the internal buffer and clears the String
 *    - str_shrink_and_release shrinks to tight fit then releases
 *
 *  Search and edits
 *    - str_find and str_rfind return SIZE_MAX when not found
 *      empty needle matches at 0 for find, at size for rfind
 *    - insert, erase, replace operate on byte positions
 *
 *  Comparisons
 *    - str_equals compares two Strings
 *    - str_equals_cstr compares to a NUL terminated cstr
 *    - str_equals_n compares to a buffer of length n
 *
 *  Errors
 *    - functions return false or NULL on allocation failure or bad args
 *    - the String stays valid and NUL terminated on failure
 *
 *  Thread safety
 *    - a String is not thread safe. do not share one instance across threads
 *
 *
 *  CUSTOMIZATION
 *  -------------
 *  Define these before including str.h to change behavior.
 *
 *  STRDEF
 *    Prefix for all function declarations and definitions.
 *    Useful values
 *      #define STRDEF static inline
 *      #define STRDEF __declspec(dllexport)
 *    default empty
 *
 *  STR_REALLOC(ptr, size) and STR_FREE(ptr)
 *    Override memory allocation.
 *    default uses realloc and free from stdlib
 *
 *  STR_START_SIZE
 *    Initial capacity when first growing from empty.
 *    counts total bytes including NUL
 *    default 64
 *
 *  STR_EXP_GROWTH_FACTOR
 *    Exponential growth factor below the linear threshold.
 *    default 2
 *
 *  STR_LIN_THRESHOLD
 *    Switch point for growth strategy in bytes.
 *    default 1 * 1024 * 1024
 *
 *  STR_LIN_GROWTH_FACTOR
 *    Linear growth step in bytes after the threshold.
 *    default 256 * 1024
 *
 *  STR_NODISCARD
 *    Marks return values as must use when C++17 or newer.
 *    define STR_IGNORE_NODISCARD to disable
 *
 *  C++ interop (opt in, only when compiling as C++)
 *    Define STR_ADD_STD_STRING to add std::string helpers
 *      str_to_std_string
 *      str_from_std_string
 *    Define STR_ADD_STD_STRING_VIEW with C++17 to add std::string_view helpers
 *      str_to_string_view
 *      str_from_string_view
 *    Headers are only included if you define these
 *
 *
 *  NOTES
 *  -----
 *  - str_append variadic macro adds a typed NULL sentinel internally
 *    you do not need to pass a final NULL
 *  - reserve takes a content length. the library adds space for the NUL
 *  - capacity equals size + 1 after str_shrink_to_fit
 *  - all APIs are binary safe. size is tracked separately from strlen
 *
 *
 *
 *  LICENSE:
 *   `sp.h` is licensed under the 3-Clause BSD license. Full license text is
 *   at the end of this file.
 **/


#ifndef STR_H_
#define STR_H_



// Ensure supported compiler is being used
#if defined(__cplusplus)
#if defined(_MSC_VER)
#if defined(_MSVC_LANG)
#if _MSVC_LANG < 201103L
#error "str.h requires at least C++11. Please use a newer compiler."
#endif
#else
#if _MSC_VER < 1900
#error "str.h requires at least C++11. Please use a newer compiler."
#endif
#endif
#else
#if __cplusplus < 201103L
#error "str.h requires at least C++11. Please use a newer compiler."
#endif
#endif
#else
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error "str.h requires at least C99. Please use a newer compiler."
#endif
#endif


// STRDEF is prefixed to all function signatures and can be defined by the user.
// A common use-case is '#define STRDEF static inline' when str.h is used in a
// single translation unit.
#ifndef STRDEF
#define STRDEF
#endif


#if !defined(STR_REALLOC) && !defined(STR_FREE)
#include <stdlib.h>
#endif

#ifndef STR_REALLOC
#define STR_REALLOC(ptr, new_size) realloc((ptr), (new_size))
#endif

#ifndef STR_FREE
#define STR_FREE(ptr) free((ptr))
#endif



#ifndef STR_START_SIZE
#define STR_START_SIZE 64u
#endif

#ifndef STR_EXP_GROWTH_FACTOR
#define STR_EXP_GROWTH_FACTOR 2 // Exponential growth factor before threshold
#endif

#ifndef STR_LIN_THRESHOLD
#define STR_LIN_THRESHOLD (1u * 1024u * 1024u) // 1 MB before switching from growth factor to linear growth
#endif

#ifndef STR_LIN_GROWTH_FACTOR
#define STR_LIN_GROWTH_FACTOR (256u * 1024u) // 256 KB after threshold
#endif



#if defined(__cplusplus) &&  __cplusplus >= 201703L && !defined(STR_IGNORE_NODISCARD)
#define STR_NODISCARD [[nodiscard]]
#else
#define STR_NODISCARD
#endif

#ifndef __cplusplus
#define STR_NO_PARAMS void
#else
#define STR_NO_PARAMS
#endif

#ifdef __cplusplus
#define STR_NOEXCEPT noexcept
#else
#define STR_NOEXCEPT
#endif

#ifdef __cplusplus
#define STR_NULL nullptr
#else
#define STR_NULL NULL
#endif

#if defined(__clang__) || defined(__GNUC__)
#  define STR_FMT(archetype, idx, first) __attribute__((format(archetype, idx, first)))
#else
#  define STR_FMT(archetype, idx, first)
#endif


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    char  *buffer;   // allocated bytes; always NUL-terminated at buffer[size] when buffer != STR_NULL
    size_t capacity; // total allocated bytes including space for NUL (>= size + 1 when buffer != STR_NULL)
    size_t size;     // number of content bytes, excluding terminating NUL
} String;



//
// Lifecycle
//

// Initialize an empty string
STR_NODISCARD STRDEF String str_init(STR_NO_PARAMS) STR_NOEXCEPT;

// Clone a string by copying the data of src into dst
STR_NODISCARD STRDEF bool str_clone(const String *src, String *dst) STR_NOEXCEPT;

// Move the contents of a string to another. 'from' gets cleared.
STR_NODISCARD STRDEF String str_move(String *from) STR_NOEXCEPT;

// Clear string without deallocating
STRDEF void str_clear(String *str) STR_NOEXCEPT;

// Free string buffer and clear
STRDEF void str_free(String *str) STR_NOEXCEPT;


//
// Ownership of raw buffer
//

// Copy the string into a new buffer.
// Returns a NUL-terminated string. Sets *out_len to the string length (excluding NUL) if not STR_NULL.
// Caller must free the returned string using STR_FREE().
STR_NODISCARD STRDEF char *str_strdup(const String *str, size_t *out_len) STR_NOEXCEPT;

// Release ownership of the internal buffer without shrinking.
// The string is cleared. Returns the internal buffer, NUL-terminated.
// Sets *out_len to the string length (excluding NUL) if not STR_NULL.
// Caller must free the returned string using STR_FREE().
STR_NODISCARD STRDEF char *str_release(String *str, size_t *out_len) STR_NOEXCEPT;

// Shrink the buffer to exactly size+1 before releasing ownership.
// The string is cleared. Returns the new buffer.
// Sets *out_len to the string length (excluding NUL) if not STR_NULL.
// Caller must free the returned string using STR_FREE().
STR_NODISCARD STRDEF char *str_shrink_and_release(String *str, size_t *out_len) STR_NOEXCEPT;


//
// Capacity management
//

// Shrink internal buffer to fit size+1, to account for trailing
// NUL-terminator.
STR_NODISCARD STRDEF bool str_shrink_to_fit(String *str) STR_NOEXCEPT;

// Reserve space for at least new_len content bytes (excluding the trailing NUL)
STR_NODISCARD STRDEF bool str_reserve(String *str, size_t new_len) STR_NOEXCEPT;


//
// Append
//

// Append one sized string to string
STR_NODISCARD STRDEF bool str_append_one_n(String *str, const char *cstr, size_t len) STR_NOEXCEPT;
// Append one NUL-terminated string to string
STR_NODISCARD STRDEF bool str_append_one(String *str, const char *cstr) STR_NOEXCEPT;

// Append several NUL-terminated strings to string.
#define str_append(str_ptr, ...) str_append_((str_ptr), __VA_ARGS__, (const char *)STR_NULL)
STR_NODISCARD STRDEF bool str_append_(String *str, const char *new_data1, ...) STR_NOEXCEPT;

// Append one character to string
STR_NODISCARD STRDEF bool str_append_char(String *str, char c) STR_NOEXCEPT;

// Append the same character n times
STR_NODISCARD STRDEF bool str_append_repeat(String *str, char c, size_t n) STR_NOEXCEPT;

// Append one string's content to another's. 'app' is appended to 'str'.
STR_NODISCARD STRDEF bool str_append_str(String *str, const String *app) STR_NOEXCEPT;

// vprintf-style append
STR_NODISCARD STRDEF bool str_vappendf(String *str, const char *fmt, va_list args) STR_NOEXCEPT  STR_FMT(printf, 2, 0);

// sprintf-style append
STR_NODISCARD STRDEF bool str_appendf(String *str, const char *fmt, ...) STR_NOEXCEPT STR_FMT(printf, 2, 3);


//
// Edits
//

// Insert and erase
STR_NODISCARD STRDEF bool str_insert_one_n(String *str, size_t pos, const char *cstr, size_t len) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_insert_one(String *str, size_t pos, const char *cstr) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_erase(String *str, size_t pos, size_t len) STR_NOEXCEPT;

// Replace a range with a string
STR_NODISCARD STRDEF bool str_replace_one_n(String *str, size_t pos, size_t len, const char *cstr, size_t slen) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_replace_one(String *str, size_t pos, size_t len, const char *cstr) STR_NOEXCEPT;


//
// Inspection, trim, search
//

// Inspect last char without removing it. Returns false if empty
STR_NODISCARD STRDEF bool str_back(const String *str, char *out_char) STR_NOEXCEPT;

// Pop last char. Returns false if empty
STR_NODISCARD STRDEF bool str_pop_back(String *str, char *out_char) STR_NOEXCEPT;

// Trim helpers
STR_NODISCARD STRDEF bool str_ltrim(String *str) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_rtrim(String *str) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_trim(String *str) STR_NOEXCEPT;

// Search for the first/last occurrence of needle.
// Returns SIZE_MAX if not found.
// If needle is empty, str_find returns 0 and str_rfind returns str->size.
STR_NODISCARD STRDEF size_t str_find_n(const String *str, const char *needle, size_t nlen) STR_NOEXCEPT;
STR_NODISCARD STRDEF size_t str_find(const String *str, const char *needle) STR_NOEXCEPT;
STR_NODISCARD STRDEF size_t str_rfind_n(const String *str, const char *needle, size_t nlen) STR_NOEXCEPT;
STR_NODISCARD STRDEF size_t str_rfind(const String *str, const char *needle) STR_NOEXCEPT;

// Return true if the two strings have the same length and contents
STR_NODISCARD STRDEF bool str_equals(const String *a, const String *b) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_equals_cstr(const String *str, const char *cstr) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_equals_n(const String *str, const char *buf, size_t n) STR_NOEXCEPT;

//
// File IO
//

STR_NODISCARD STRDEF bool str_write_file(const String *str, FILE *f) STR_NOEXCEPT;
STR_NODISCARD STRDEF bool str_read_file(String *str, FILE *f) STR_NOEXCEPT;


#ifdef __cplusplus
} // extern "C"
#endif



///
// C++ std::string and std::string_view conversion functions
//

#if defined(__cplusplus)
#if defined(STR_ADD_STD_STRING)
#include <string>

// copies to std::string
STR_NODISCARD STRDEF std::string
str_to_std_string(const String &str);

// copies from std::string
STR_NODISCARD STRDEF String
str_from_std_string(const std::string &s);
#endif // STR_ADD_STD_STRING


#if __cplusplus >= 201703L && defined(STR_ADD_STD_STRING_VIEW)
#include <string_view>

// zero-copy view over String
STR_NODISCARD STRDEF std::string_view
str_to_string_view(const String &str);

// copies from std::string_view
STR_NODISCARD STRDEF String
str_from_string_view(std::string_view sv);
#endif // C++17 && STR_ADD_STD_STRING_VIEW
#endif // __cplusplus


#ifdef STR_IMPLEMENTATION

#include <ctype.h>
#include <stdint.h>
#include <string.h>

STRDEF String
str_init(STR_NO_PARAMS) STR_NOEXCEPT
{
    String result = {STR_NULL, 0, 0};

    char *p = (char *)STR_REALLOC(STR_NULL, 1);
    if (p) {
        p[0] = '\0';
        result.buffer = p;
        result.capacity = 1;
        result.size = 0;
    }

    return result;
}

STRDEF bool
str_clone(const String *src, String *dst) STR_NOEXCEPT
{
    if (src == dst) return true;

    if (!dst) return false;

    str_clear(dst);

    if (!src) return true;
    if (src->size == 0) return true;

    if (!str_reserve(dst, src->size)) return false;
    memcpy(dst->buffer, src->buffer, src->size + 1); // include '\0'
    dst->size = src->size;
    return true;
}

STRDEF String
str_move(String *from) STR_NOEXCEPT
{
    if (!from) {
        return str_init();
    }

    String result = *from;

    *from = str_init();
    return result;
}

STRDEF void
str_clear(String *str) STR_NOEXCEPT
{
    if (!str) return;

    str->size = 0;

    if (str->buffer) {
        str->buffer[0] = '\0';
    } else {
        char *p = (char *)STR_REALLOC(STR_NULL, 1);
        if (p) {
            p[0] = '\0';
            str->buffer = p;
            str->capacity = 1;
            str->size = 0;
        }
    }
}

STRDEF void
str_free(String *str) STR_NOEXCEPT
{
    if (!str) return;

    STR_FREE(str->buffer);
    str->buffer   = STR_NULL;
    str->capacity = 0;
    str->size     = 0;
}

STRDEF char *
str_strdup(const String *str, size_t *out_len) STR_NOEXCEPT
{
    size_t len = (str && str->buffer) ? str->size : 0;

    if (len + 1 < len) return STR_NULL; // Overflow protection

    char *buf = (char *)STR_REALLOC(STR_NULL, len + 1);
    if (!buf) return STR_NULL;

    if (len) memcpy(buf, str->buffer, len);
    buf[len] = '\0';

    if (out_len) *out_len = len;
    return buf;
}

STRDEF char *
str_release(String *str, size_t *out_len) STR_NOEXCEPT
{
    if (!str) return STR_NULL;

    if (!str->buffer) {
        char *z = (char *)STR_REALLOC(STR_NULL, 1);
        if (!z) return STR_NULL;
        z[0] = '\0';
        if (out_len) *out_len = 0;
        return z;
    }

    str->buffer[str->size] = '\0';

    if (out_len) *out_len = str->size;
    char *result = str->buffer;

    str->buffer   = STR_NULL;
    str->capacity = 0;
    str->size     = 0;

    return result;
}

STRDEF char *
str_shrink_and_release(String *str, size_t *out_len) STR_NOEXCEPT
{
    if (!str) return STR_NULL;

    if (!str->buffer) {
        char *z = (char *)STR_REALLOC(STR_NULL, 1);
        if (!z) return STR_NULL;
        z[0] = '\0';
        if (out_len) *out_len = 0;
        return z;
    }

    size_t need = str->size + 1;
    if (need < str->size) return STR_NULL; // Overflow protection

    void *p = STR_REALLOC(str->buffer, need);
    if (p) {
        str->buffer   = (char *)p;
        str->capacity = need;
        str->buffer[str->size] = '\0';
    } else {
        str->buffer[str->size] = '\0';
    }

    if (out_len) *out_len = str->size;
    char *result = str->buffer;

    str->buffer   = STR_NULL;
    str->capacity = 0;
    str->size     = 0;

    return result;
}

STRDEF bool
str_shrink_to_fit(String *str) STR_NOEXCEPT
{
    if (!str) return false;
    if (!str->buffer) return true;

    if (str->capacity == str->size + 1) {
        str->buffer[str->size] = '\0';
        return true;
    }

    size_t need = str->size + 1;
    if (need < str->size) return false; // Overflow protection

    void *p = STR_REALLOC(str->buffer, need);
    if (!p) return false;

    str->buffer   = (char *)p;
    str->capacity = need;
    str->buffer[str->size] = '\0';
    return true;
}

static inline bool
str_would_overflow_(size_t a, size_t b)
{
    return b > SIZE_MAX - a;
}

static inline bool
str_grow_to_fit_(String *str, size_t n) STR_NOEXCEPT
{
    if (!str) return false;

    size_t np1 = n + 1; // 'n plus 1' to account for trailing NUL-terminator
    if (np1 < n) return false; // Overflow protection

    if (str->buffer && np1 <= str->capacity) {
        return true;
    }

    size_t new_cap = str->capacity ? str->capacity : (STR_START_SIZE > 1 ? STR_START_SIZE : 1);

    // Exponential growth until threshold
    while (new_cap < np1 && new_cap < STR_LIN_THRESHOLD) {
        if (new_cap > SIZE_MAX / STR_EXP_GROWTH_FACTOR) { new_cap = SIZE_MAX; break; } // Overflow protection
        new_cap *= STR_EXP_GROWTH_FACTOR;
    }

    // Linear growth after threshold
    while (new_cap < np1) {
        if (new_cap > SIZE_MAX - STR_LIN_GROWTH_FACTOR) { new_cap = SIZE_MAX; break; } // Overflow protection
        new_cap += STR_LIN_GROWTH_FACTOR;
    }

    void *new_buffer = STR_REALLOC(str->buffer, new_cap);
    if (!new_buffer) {
        return false;
    }

    str->buffer = (char *)new_buffer;
    str->capacity = new_cap;

    str->buffer[str->size] = '\0';

    return true;
}

STRDEF bool
str_reserve(String *str, size_t new_len) STR_NOEXCEPT
{
    if (!str) return false;
    if (!str_grow_to_fit_(str, new_len)) return false;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_append_one_n(String *str, const char *cstr, size_t len) STR_NOEXCEPT
{
    if (!str || (!cstr && len)) return false;

    if (str_would_overflow_(str->size, len)) return false;
    if (!str_grow_to_fit_(str, str->size + len)) return false;

    if (len) memcpy(str->buffer + str->size, cstr, len);
    str->size += len;
    str->buffer[str->size] = '\0';

    return true;
}

STRDEF bool
str_append_one(String *str, const char *cstr) STR_NOEXCEPT
{
    if (!str || !cstr) return false;
    size_t len = strlen(cstr);
    return str_append_one_n(str, cstr, len);
}

STRDEF bool
str_append_(String *str, const char *new_data1, ...) STR_NOEXCEPT
{
    if (!str) return false;

    va_list args;
    va_start(args, new_data1);

    bool ok = true;
    const char *data = new_data1;
    while (ok && data != STR_NULL) {
        if (!str_append_one(str, data)) ok = false;
        data = va_arg(args, const char *);
    }

    va_end(args);

    return ok;
}

STRDEF bool
str_append_char(String *str, char c) STR_NOEXCEPT
{
    if (!str) return false;
    if (str_would_overflow_(str->size, 1)) return false;
    if (!str_grow_to_fit_(str, str->size + 1)) return false;
    str->buffer[str->size++] = c;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_append_repeat(String *str, char c, size_t n) STR_NOEXCEPT
{
    if (!str) return false;
    if (n == 0) return true;
    if (str_would_overflow_(str->size, n)) return false;
    if (!str_grow_to_fit_(str, str->size + n)) return false;

    memset(str->buffer + str->size, (unsigned char)c, n);
    str->size += n;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_append_str(String *str, const String *app) STR_NOEXCEPT
{
    if (!str) return false;
    if (!app || app->size == 0) return true;

    if (str == app) {
        size_t len = str->size;
        if (!str_grow_to_fit_(str, str->size + len)) return false;
        // source and destination are the same allocation, so use memmove
        memmove(str->buffer + str->size, str->buffer, len);
        str->size += len;
        str->buffer[str->size] = '\0';
        return true;
    }

    return str_append_one_n(str, app->buffer, app->size);
}

STRDEF bool
str_vappendf(String *str, const char *fmt, va_list args) STR_NOEXCEPT
{
    if (!str || !fmt) return false;

    va_list ap;
    va_copy(ap, args);
    int need = vsnprintf(STR_NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return false;

    if (!str_grow_to_fit_(str, str->size + (size_t)need)) return false;

    va_copy(ap, args);
    vsnprintf(str->buffer + str->size, (size_t)need + 1, fmt, ap);
    va_end(ap);

    str->size += (size_t)need;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_appendf(String *str, const char *fmt, ...) STR_NOEXCEPT
{
    if (!str || !fmt) return false;
    va_list args;
    va_start(args, fmt);
    bool ok = str_vappendf(str, fmt, args);
    va_end(args);
    return ok;
}

STRDEF bool
str_insert_one_n(String *str, size_t pos, const char *cstr, size_t len) STR_NOEXCEPT
{
    if (!str) return false;
    if (pos > str->size) return false;
    if (!cstr && len) return false;
    if (len == 0) return true;

    if (str_would_overflow_(str->size, len)) return false;
    if (!str_grow_to_fit_(str, str->size + len)) return false;

    size_t tail = str->size - pos;
    memmove(str->buffer + pos + len, str->buffer + pos, tail);
    if (len) memcpy(str->buffer + pos, cstr, len);

    str->size += len;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_insert_one(String *str, size_t pos, const char *cstr) STR_NOEXCEPT
{
    if (!str || !cstr) return false;
    size_t len = strlen(cstr);
    return str_insert_one_n(str, pos, cstr, len);
}

STRDEF bool
str_erase(String *str, size_t pos, size_t len) STR_NOEXCEPT
{
    if (!str) return false;
    if (pos > str->size) return false;
    if (len == 0) return true;

    size_t remain = str->size - pos;
    if (len > remain) len = remain;

    size_t end  = pos + len;
    size_t tail = str->size - end;

    if (tail) memmove(str->buffer + pos, str->buffer + end, tail);

    str->size -= len;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_replace_one_n(String *str, size_t pos, size_t len, const char *cstr, size_t slen) STR_NOEXCEPT
{
    if (!str) return false;
    if (pos > str->size) return false;
    if (!cstr && slen) return false;

    // Calculate target end after erase
    size_t end = pos + len;
    if (end > str->size) end = str->size;
    size_t cut = end - pos;

    // Resize to new total size
    size_t tmp = str->size - cut;
    if (str_would_overflow_(tmp, slen)) return false;
    size_t new_size = tmp + slen;
    if (!str_grow_to_fit_(str, new_size)) return false;

    // Move tail to its new position if needed
    size_t old_tail = str->size - end;
    if (old_tail && (slen != cut)) {
        memmove(str->buffer + pos + slen, str->buffer + end, old_tail);
    }

    if (slen) memcpy(str->buffer + pos, cstr, slen);

    str->size = new_size;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_replace_one(String *str, size_t pos, size_t len, const char *cstr) STR_NOEXCEPT
{
    if (!str) return false;
    size_t slen = strlen(cstr);
    return str_replace_one_n(str, pos, len, cstr, slen);
}

STRDEF bool
str_back(const String *str, char *out_char) STR_NOEXCEPT
{
    if (!str || str->size == 0) return false;
    if (out_char) *out_char = str->buffer[str->size - 1];
    return true;
}

STRDEF bool
str_pop_back(String *str, char *out_char) STR_NOEXCEPT
{
    if (!str || str->size == 0) return false;
    if (out_char) *out_char = str->buffer[str->size - 1];
    str->size -= 1;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_ltrim(String *str) STR_NOEXCEPT
{
    if (!str || str->size == 0) return true;

    size_t i = 0;
    while (i < str->size && isspace((unsigned char)str->buffer[i])) i++;

    if (i == 0) return true;
    size_t remain = str->size - i;
    memmove(str->buffer, str->buffer + i, remain);
    str->size = remain;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_rtrim(String *str) STR_NOEXCEPT
{
    if (!str || str->size == 0) return true;

    size_t i = str->size;
    while (i > 0 && isspace((unsigned char)str->buffer[i - 1])) i--;

    if (i == str->size) return true;
    str->size = i;
    str->buffer[str->size] = '\0';
    return true;
}

STRDEF bool
str_trim(String *str) STR_NOEXCEPT
{
    if (!str) return false;
    bool ok = str_rtrim(str);
    if (!ok) return false;
    return str_ltrim(str);
}

STRDEF size_t
str_find_n(const String *str, const char *needle, size_t nlen) STR_NOEXCEPT
{
    if (!str || !str->buffer) return SIZE_MAX;
    if (!needle) return SIZE_MAX;
    if (nlen == 0) return 0;
    if (str->size == 0 || nlen > str->size) return SIZE_MAX;

    const char *hay = str->buffer;
    size_t last = str->size - nlen;

    for (size_t i = 0; i <= last; ++i) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, nlen) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

STRDEF size_t
str_find(const String *str, const char *needle) STR_NOEXCEPT
{
    if (!needle) return SIZE_MAX;
    return str_find_n(str, needle, strlen(needle));
}

STRDEF size_t
str_rfind_n(const String *str, const char *needle, size_t nlen) STR_NOEXCEPT
{
    if (!str || !str->buffer) return SIZE_MAX;
    if (!needle) return SIZE_MAX;
    if (nlen == 0) return str->size;
    if (str->size == 0 || nlen > str->size) return SIZE_MAX;

    const char *hay = str->buffer;
    for (size_t i = str->size - nlen + 1; i-- > 0; ) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, nlen) == 0) {
            return i;
        }
        if (i == 0) break;
    }
    return SIZE_MAX;
}

STRDEF size_t
str_rfind(const String *str, const char *needle) STR_NOEXCEPT
{
    if (!needle) return SIZE_MAX;
    return str_rfind_n(str, needle, strlen(needle));
}

STRDEF bool
str_equals(const String *a, const String *b) STR_NOEXCEPT
{
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->size != b->size) return false;
    if (a->size == 0) return true;
    return memcmp(a->buffer, b->buffer, a->size) == 0;
}

STRDEF bool
str_equals_cstr(const String *str, const char *cstr) STR_NOEXCEPT
{
    if (!str || !cstr) return false;
    size_t len = strlen(cstr);
    return str_equals_n(str, cstr, len);
}

STRDEF bool
str_equals_n(const String *str, const char *buf, size_t n) STR_NOEXCEPT
{
    if (!str || !buf) return false;
    if (str->size != n) return false;
    if (n == 0) return true;
    return memcmp(str->buffer, buf, n) == 0;
}

STRDEF bool
str_write_file(const String *str, FILE *f) STR_NOEXCEPT
{
    if (!str || !f) return false;
    if (str->size == 0) return true;
    size_t n = fwrite(str->buffer, 1, str->size, f);
    return n == str->size;
}

STRDEF bool
str_read_file(String *str, FILE *f) STR_NOEXCEPT
{
    if (!str || !f) return false;

    char buf[32768];
    for (;;) {
        size_t r = fread(buf, 1, sizeof(buf), f);
        if (r > 0) {
            if (!str_append_one_n(str, buf, r)) return false;
        }
        if (r < sizeof(buf)) {
            if (feof(f)) break;
            if (ferror(f)) return false;
        }
    }
    return true;
}



#if defined(__cplusplus)
#if defined(STR_ADD_STD_STRING)

STRDEF std::string
str_to_std_string(const String &str)
{
    const char *p = str.buffer ? str.buffer : "";
    return std::string(p, str.size);
}

STRDEF String
str_from_std_string(const std::string &s)
{
    String out = str_init();
    if (s.empty()) return out;

    if (!str_reserve(&out, s.size())) {
        return out; // OOM -> empty
    }

    memcpy(out.buffer, s.data(), s.size());
    out.size = s.size();
    out.buffer[out.size] = '\0';
    return out;
}
#endif // STR_ADD_STD_STRING

#if __cplusplus >= 201703L && defined(STR_ADD_STD_STRING_VIEW)

STRDEF std::string_view
str_to_string_view(const String &str)
{
    return std::string_view(str.buffer ? str.buffer : "", str.size);
}

STRDEF String
str_from_string_view(std::string_view sv)
{
    String out = str_init();
    if (sv.empty()) return out;

    if (!str_reserve(&out, sv.size())) {
        return out;
    }
    memcpy(out.buffer, sv.data(), sv.size());
    out.size = sv.size();
    out.buffer[out.size] = '\0';
    return out;
}
#endif // C++17 && STR_ADD_STD_STRING_VIEW
#endif // __cplusplus


#endif // STR_IMPLEMENTATION

#endif // STR_H_


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
