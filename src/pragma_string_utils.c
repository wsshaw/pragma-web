#include "pragma_poison.h"

/**
 * append(): Append `string` to `result`, optionally tracking position via *j.
 *
 * Copies `string` to `result` starting at index *j (if provided) or at the
 * current terminator (wcslen(result)). Updates *j on success.
 *
 * arguments:
 *  wchar_t *string (text to append; must not be NULL)
 *  wchar_t *result (destination buffer; must not be NULL and large enough)
 *  size_t  *j      (optional in/out index; may be NULL)
 *
 * returns:
 *  void
 */
void append(wchar_t *string, wchar_t *result, size_t *j) {
	if (string == NULL || result == NULL) {
		printf("! Invalid arguments in append(): string argument is%s null; result is%s null\n",
			!string ? "" : " not", !result ? "" : " not" );
			return;
	}

	size_t start = j ? *j : wcslen(result);
	size_t length = wcslen(string);

	if (start + length < start || start + length >= SIZE_MAX) {
		printf("! buffer overflow in append()\n");
			return;
	}

	for (size_t i = 0; i < length; i++)
		result[start + i] = string[i];

	if (j)
		*j += length;
	else
		result[start + length] = L'\0';
}

/**
 * char_convert(): Convert a wide-character string to a newly allocated narrow string.
 *
 * Uses wcstombs() to produce a UTF-8/locale-encoded char*.
 *
 * arguments:
 *  const wchar_t *w (source wide string; must not be NULL)
 *
 * returns:
 *  char* (heap-allocated narrow string; NULL on allocation failure)
 */
char* char_convert(const wchar_t* w) {
	size_t len = wcstombs(NULL, w, 0);
	if (len == (size_t)-1) {
		perror("! Error: invalid wide character sequence in char_convert()");
		return NULL;
	}

	char *out_str = malloc(len + 1);

	if (out_str != NULL) {
		size_t result = wcstombs(out_str, w, len + 1);
		if (result == (size_t)-1) {
			perror("! Error: conversion failed in char_convert()");
			free(out_str);
			return NULL;
		}
		return out_str;
	} else {
		printf("! Error: malloc() failed while converting string types in char_convert()!\n");
		return NULL;
	}
}

/**
 * wchar_convert(): Convert a narrow string to a newly allocated wide-character string.
 *
 * Uses mbstowcs() to produce a wchar_t*.
 *
 * arguments:
 *  const char *c (source char*; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated wide string; NULL on error)
 */
wchar_t* wchar_convert(const char* c) {

	size_t len = mbstowcs(NULL, c, 0);

	if (len < 0) {
		perror("Can't convert character string to wide characters (wchar_convert()): ");
		return NULL;
	}

	wchar_t* w = malloc((len + 1) * sizeof(wchar_t));

	if (w == NULL) {
		perror("malloc() failure in wchar_convert!");
		return NULL;
	}

	if (mbstowcs(w, c, len + 1) < 0 ) {
        	perror("Error converting character to wide char: ");
		free(w);
        	return NULL;
	}

	return w;

}

/**
 * replace_substring(): Replace the first occurrence of `find` with `replace` in `str`.
 *
 * Allocates and returns a new buffer containing the result; original `str` is unchanged.
 *
 * arguments:
 *  wchar_t       *str     (source string; must not be NULL)
 *  const wchar_t *find    (substring to locate; must not be NULL)
 *  const wchar_t *replace (replacement text; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated result; NULL on error)
 */
wchar_t* replace_substring(wchar_t *str, const wchar_t *find, const wchar_t *replace) {
	if (str == NULL || find == NULL || replace == NULL)
		return NULL;

	wchar_t *pos = wcsstr(str, find); // location of requested substr; bail if none
	if (pos == NULL)
		return wcsdup(str);  // Return copy for consistent memory management

	// new string length: it's the existing length minus substring to replace plus replacement length
	size_t new_string_length = wcslen(str) - wcslen(find) + wcslen(replace);
	wchar_t *new_str = (wchar_t *)malloc((new_string_length + 1) * sizeof(wchar_t));

	if (!new_str) {
		printf("! Error: malloc failed in replace_substring\n");
		return NULL;
	}

	// build the resulting string
	wmemcpy(new_str, str, pos - str);
	wmemcpy(new_str + (pos-str), replace, wcslen(replace));
	wcscpy(new_str + (pos - str) + wcslen(replace), pos + wcslen(find));

	return new_str;
}

/**
 * strip_terminal_newline(): Remove a single trailing newline from a wide character string (s)
 * and/or a narrow one (t). You can convceivably pass both arguments, and it will work fine,
 * but either can be null.
 *
 * If provided, trims the last character if it is '\n' (or L'\n' for wide string).
 *
 * arguments:
 *  wchar_t *s (optional wide string to trim; may be NULL)
 *  char    *t (optional narrow string to trim; may be NULL)
 *
 * returns:
 *  void
 */
void strip_terminal_newline(wchar_t *s, char *t) {
	if (!s && !t)
		return;

	size_t marker = 0;

	if (s) {
		marker = wcslen(s);
		if (marker > 0 && s[marker - 1] == L'\n') {
			s[marker - 1] = L'\0';
		}
	}
	if (t) {
		marker = strlen(t);
		if (marker > 0 && t[marker - 1] == '\n') {
			t[marker - 1] = '\0';
		}
	}

}

/**
 * string_from_int(): Convert a long integer to a newly allocated wide-character string.
 *
 * arguments:
 *  long int n (value to convert)
 *
 * returns:
 *  wchar_t* (heap-allocated numeric string)
 */
wchar_t* string_from_int(long int n) {
	int c = 0;
	long int x = n;

	// Handle zero case properly
	if (x == 0) {
		c = 1;
	} else {
		// Handle negative numbers
		if (x < 0) {
			c = 1; // for the minus sign
			x = -x;
		}

		while (x != 0) {
			x = x/10;
			c++;
		}
	}

	wchar_t* str = malloc((c + 1) * sizeof(wchar_t));
	if (!str) return NULL;

	swprintf(str, c + 1, L"%ld", n);

	return str;
}

/**
 * wrap_with_element(): Surround text with `start` and `close` and return a new buffer.
 *
 * Allocates a buffer of size len(start) + len(text) + len(close) + 1 and concatenates.
 * Caller owns and must free.
 *
 * arguments:
 *  wchar_t *text  (inner text; must not be NULL)
 *  wchar_t *start (prefix/opening element; must not be NULL)
 *  wchar_t *close (suffix/closing element; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated combined string; NULL on allocation failure)
 */
wchar_t* wrap_with_element(wchar_t* text, wchar_t* start, wchar_t* close) {
	wchar_t *output = malloc(( wcslen(text) + wcslen(start) + wcslen(close) + 1 ) * sizeof(wchar_t));

	if (!output) {
		perror("! malloc() in wrap_with_element:");
		return NULL;
	}

	wcscpy(output, start);
	wcscat(output, text);
	wcscat(output, close);

	return output;
}