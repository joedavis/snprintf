/*
 * Simple freestanding implementation of snprintf and vsnprintf.
 *
 * Will not support (for simplicity):
 *     - Floating point.
 *     - %n
 *     - %m$ and *n$ style format specifiers
 *     - wide characters
 *     - ' format flag (thousands separator)
 *     - * precision (specifying precision in the next argument, this should
 *       be easy enough to implement, but I don't need it)
 *
 * Copyright (c) 2018, Joe Davis <me@jo.ie>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#include "snprintf.h"

#define MAX_INT_FIELD_WIDTH 128

#if !__has_include(<string.h>)
static size_t strnlen(const char *s, size_t n);
static size_t strlen(const char *s, size_t n);
#else
#include <string.h>
#endif

enum length_modifier {
	LEN_CHAR,       // 'hh'
	LEN_SHORT,      // 'h'
	LEN_INT,        // Default
	LEN_LONG,       // 'l'
	LEN_LONG_LONG,  // 'll'
	LEN_INTMAX_T,   // 'j'
	LEN_SIZE_T,     // 'z'
	LEN_PTRDIFF_T,  // 't'
};

enum flags {
	LEFT_JUSTIFY  = (1 << 0), // -
	SIGN          = (1 << 1), // +
	SIGN_SPACE    = (1 << 2), // ' '
	BASE_PREFIX   = (1 << 3), // #
	ZERO_PAD      = (1 << 4), // 0
	SIGNED        = (1 << 5),
	UPPER         = (1 << 6)
};

static void put_char(char *buf, size_t sz, size_t *idx, char c);
static int get_flags(const char *restrict *fmt);
static size_t get_field_width(const char *restrict *fmt);
static size_t get_precision(const char *restrict *fmt);
static enum length_modifier get_length(const char *restrict *fmt);
static bool is_flag(char c);
static bool is_digit(char c);

#define va_arg_int(ap, length, i) do { \
	switch (length) { \
	case LEN_CHAR: \
	case LEN_SHORT: \
	case LEN_INT: \
		i = va_arg(ap, unsigned int); \
		break; \
	case LEN_LONG: \
		i = va_arg(ap, unsigned long); \
		break; \
	case LEN_LONG_LONG: \
		i = va_arg(ap, unsigned long long); \
		break; \
	case LEN_INTMAX_T: \
		i = va_arg(ap, uintmax_t); \
		break; \
	case LEN_SIZE_T: \
		i = va_arg(ap, size_t); \
		break; \
	case LEN_PTRDIFF_T: \
		i = va_arg(ap, ptrdiff_t); \
		break; \
	} \
} while (0)

static void fmt_int(char *buf, size_t sz, size_t *idx, uintmax_t n,
    int flags, size_t field_width, size_t precision, enum length_modifier length,
    int base);
static void fmt_string(char *buf, size_t sz, size_t *idx, const char *s,
    int flags, size_t field_width, size_t precision);

int
snprintf(char *restrict buf, size_t sz, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int written = vsnprintf(buf, sz, fmt, ap);
	va_end(ap);
	return written;
}

int
vsnprintf(char *restrict buf, size_t sz, const char *restrict fmt, va_list ap)
{
	size_t idx = 0;
	while (*fmt) {
		if (*fmt != '%' || *++fmt == '%') {
			put_char(buf, sz, &idx, *fmt);
			++fmt;
			continue;
		}

		int flags = get_flags(&fmt);
		size_t width = get_field_width(&fmt);
		size_t precision = get_precision(&fmt);
		enum length_modifier length = get_length(&fmt);
		uintmax_t n = 0;
		const char *s = NULL;
		void *p = NULL;

		// We can use the same function to format strings and lone
		// characters if we store the char in an array:
		char c[2] = { 0 };

		switch (*fmt) {
		case 'd':
		case 'i':
			flags |= SIGNED;
		case 'u':
			va_arg_int(ap, length, n);
			fmt_int(buf, sz, &idx, n, flags, width, precision,
			    length, 10);
			break;

		case 'X':
			flags |= UPPER;
		case 'x':
			va_arg_int(ap, length, n);
			fmt_int(buf, sz, &idx, n, flags, width, precision,
			    length, 16);
			break;

		case 'o':
			va_arg_int(ap, length, n);
			fmt_int(buf, sz, &idx, n, flags, width, precision,
			    length, 8);
			break;

		case 'c':
			c[0] = va_arg(ap, int);
			fmt_string(buf, sz, &idx, c, flags, width, precision);
			break;

		case 's':
			s = va_arg(ap, const char *);
			fmt_string(buf, sz, &idx, s, flags, width, precision);
			break;

		case 'p':
			p = va_arg(ap, void *);
			n = (uintptr_t) p;
			flags |= BASE_PREFIX;
			fmt_int(buf, sz, &idx, n, flags, width, precision,
			    LEN_INTMAX_T, 16);
			break;
		}
		++fmt;
	}
	put_char(buf, sz, &idx, 0);
	buf[sz-1] = '\0'; // Ensure NULL termination
	return idx-1;
}

static void
put_char(char *restrict buf, size_t sz, size_t *idx, char c)
{
	if (*idx < sz)
		buf[*idx] = c;
	++*idx;
}

static int
get_flags(const char *restrict *fmt)
{
	int flags = 0;
	while (is_flag(**fmt)) {
		switch (**fmt) {
		case '-':
			flags |= LEFT_JUSTIFY;
			break;
		case '+':
			flags |= SIGN;
			break;
		case ' ':
			flags |= SIGN_SPACE;
			break;
		case '#':
			flags |= BASE_PREFIX;
			break;
		case '0':
			flags |= ZERO_PAD;
			break;
		}

		++*fmt;
	}
	if ((flags & LEFT_JUSTIFY) && (flags & ZERO_PAD))
		flags ^= ZERO_PAD;
	return flags;
}

static bool
is_flag(char c)
{
	return c == '\'' || c == '-' || c == '+' || c == ' ' || c == '#' ||
	    c == '0';
}

static bool
is_digit(char c)
{
	return '0' <= c && c <= '9';
}

static size_t
simple_atoi(const char *restrict *fmt)
{
	size_t n = 0;
	while (is_digit(**fmt)) {
		n *= 10;
		n += **fmt - '0';
		++*fmt;
	}
	return n;
}

static size_t
get_field_width(const char *restrict *fmt)
{
	return simple_atoi(fmt);
}

static size_t
get_precision(const char *restrict *fmt)
{
	if (**fmt != '.')
		return 0;
	++*fmt;
	return simple_atoi(fmt);
}

static enum length_modifier
get_length(const char *restrict *fmt)
{
	switch (**fmt) {
	case 'h':
		++*fmt;
		if (**fmt == 'h') {
			++*fmt;
			return LEN_CHAR;
		} else {
			return LEN_SHORT;
		}
	case 'l':
		++*fmt;
		if (**fmt == 'l') {
			++*fmt;
			return LEN_LONG_LONG;
		} else {
			return LEN_LONG;
		}
	case 'j':
		++*fmt;
		return LEN_INTMAX_T;
	case 'z':
		++*fmt;
		return LEN_SIZE_T;
		break;
	case 't':
		++*fmt;
		return LEN_PTRDIFF_T;
	default:
		return LEN_INT;
	}
}

static void
fmt_int(char *out, size_t sz, size_t *idx, uintmax_t n, int flags,
    size_t field_width, size_t precision, enum length_modifier length,
    int base)
{
	static const char lower_digits[] = "0123456789abcdef";
	static const char upper_digits[] = "012345689ABCDEF";

	const char *digits = (flags & UPPER) ? upper_digits : lower_digits;
	char buf[MAX_INT_FIELD_WIDTH] = { 0 };
	char sign = 0;
	size_t i = 0;

	if (precision)
		flags &= ~ZERO_PAD;

	char pad_char = (flags & ZERO_PAD) ? '0' : ' ';

	if (flags & SIGNED) {
		switch (length) {
#define CASE(len, type) \
		case len: \
			if (((type) n) < 0) { \
				sign = '-'; \
				n = -(type) n; \
			} \
			break;
		CASE(LEN_CHAR, signed char)
		CASE(LEN_SHORT, short)
		CASE(LEN_INT, int)
		CASE(LEN_LONG, long)
		CASE(LEN_LONG_LONG, long long)
		CASE(LEN_PTRDIFF_T, ptrdiff_t)
		default: // Silence GCC warnings
			break;
#undef CASE
		}
	}

	if (flags & SIGN && !sign)
		sign = '+';
	else if (flags & SIGN_SPACE && !sign)
		sign = ' ';

	do {
		put_char(buf, sizeof buf, &i, digits[n % base]);
		n /= base;
		if (precision) --precision;
	} while (n > 0 || precision);

	if (sign)
		put_char(buf, sizeof buf, &i, sign);

	if (flags & BASE_PREFIX && base == 16) {
		if (flags & ZERO_PAD) {
			field_width -= 2;
		} else {
			put_char(buf, sizeof buf, &i, flags & UPPER ? 'X' : 'x');
			put_char(buf, sizeof buf, &i, '0');
		}
	} else if (flags & BASE_PREFIX && base == 8) {
		put_char(buf, sizeof buf, &i, '0');
	}

	size_t j = i;
	if (!(flags & LEFT_JUSTIFY)) {
		while (i < field_width)
			put_char(buf, sizeof buf, &i, pad_char);

		if (flags & BASE_PREFIX && base == 16 && flags & ZERO_PAD) {
			put_char(buf, sizeof buf, &i, flags & UPPER ? 'X' : 'x');
			put_char(buf, sizeof buf, &i, '0');
		}
	}

	if (i > sizeof buf)
		i = sizeof buf;

	while (i--)
		put_char(out, sz, idx, buf[i]);

	if (flags & LEFT_JUSTIFY) {
		for (; j < field_width; j++) {
			put_char(out, sz, idx, ' ');
			j++;
		}
	}
}

static void
fmt_string(char *buf, size_t sz, size_t *idx, const char *s, int flags,
    size_t field_width, size_t precision)
{
	size_t len = precision ? strnlen(s, precision) : strlen(s);

	if (!(flags & LEFT_JUSTIFY)) {
		for (size_t i = len; i < field_width; i++)
			put_char(buf, sz, idx, ' ');
	}

	for (size_t i = 0; i < len; i++)
		put_char(buf, sz, idx, s[i]);

	if (flags & LEFT_JUSTIFY) {
		for (size_t i = len; i < field_width; i++)
			put_char(buf, sz, idx, ' ');
	}
}

#if !__has_include(<string.h>)
static size_t
strnlen(const char *s, size_t n)
{
	size_t i = 0;
	while (s[i] && i < n) i++;
	return i;
}

static size_t
strlen(const char *s, size_t n)
{
	size_t i = 0;
	while (s) i++;
	return i;
}
#endif

