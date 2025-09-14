#include "pragma_poison.h"

/**
 * split_before(): Copy the portion of `input` occurring before `delim` into `output`.
 *
 * If `delim` is not found, copies the entire input. Ensures output is null-terminated!
 *
 * arguments:
 *  wchar_t       *delim  (delimiter to search for; must not be NULL)
 *  const wchar_t *input  (input string; must not be NULL)
 *  wchar_t       *output (destination buffer; must not be NULL and large enough)
 *
 * returns:
 *  bool (true if delimiter found and split performed; false if not found)
 */
bool split_before(wchar_t *delim, const wchar_t *input, wchar_t *output) {
	wchar_t *delimiter_pos = wcsstr(input, delim);
	if (delimiter_pos != NULL) {
		size_t length_before_delimiter = delimiter_pos - input;
		wcsncpy(output, input, length_before_delimiter);
		output[length_before_delimiter] = L'\0';
		return true;
	} else {
		wcscpy(output, input);  // delimiter not found, so just return the input
    }
	return false;
}

/**
 * strip_html_tags(): Remove HTML/XML tags from a wide string.
 *
 * Removes all content between < and > characters, also converts common
 * HTML entities to their text equivalents.
 *
 * Memory:
 *  Returns a heap-allocated wide-character buffer containing the stripped text.
 *  Caller is responsible for free().
 *
 * arguments:
 *  const wchar_t *input (the input string to strip; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated stripped buffer on success; NULL on error)
 */
wchar_t* strip_html_tags(const wchar_t *input) {
    if (!input) {
        return NULL;
    }

    size_t input_len = wcslen(input);
    wchar_t *result = malloc((input_len + 1) * sizeof(wchar_t));
    if (!result) {
        return NULL;
    }

    size_t result_pos = 0;
    bool in_tag = false;

    for (size_t i = 0; i < input_len; i++) {
        if (input[i] == L'<') {
            in_tag = true;
        } else if (input[i] == L'>') {
            in_tag = false;
        } else if (!in_tag) {
            // Convert common HTML entities
            if (i + 3 < input_len && wcsncmp(&input[i], L"&lt;", 4) == 0) {
                result[result_pos++] = L'<';
                i += 3; // Skip the rest of &lt;
            } else if (i + 3 < input_len && wcsncmp(&input[i], L"&gt;", 4) == 0) {
                result[result_pos++] = L'>';
                i += 3; // Skip the rest of &gt;
            } else if (i + 4 < input_len && wcsncmp(&input[i], L"&amp;", 5) == 0) {
                result[result_pos++] = L'&';
                i += 4; // Skip the rest of &amp;
            } else if (i + 5 < input_len && wcsncmp(&input[i], L"&quot;", 6) == 0) {
                result[result_pos++] = L'"';
                i += 5; // Skip the rest of &quot;
            } else {
                result[result_pos++] = input[i];
            }
        }
    }

    result[result_pos] = L'\0';
    return result;
}

/**
 * get_page_description(): Generate a description for a page.
 *
 * Uses the summary field if available, otherwise falls back to the first 240
 * characters of the page content (or entire content if less than 240 chars).
 * Strips HTML tags from auto-generated descriptions.
 *
 * Memory:
 *  Returns a heap-allocated wide-character buffer containing the description.
 *  Caller is responsible for free().
 *
 * arguments:
 *  pp_page *page (the page to generate description for; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated description buffer on success; NULL on error)
 */
wchar_t* get_page_description(pp_page *page) {
    if (!page) {
        return NULL;
    }

    // Use summary if available and not empty
    if (page->summary && wcslen(page->summary) > 0) {
        wchar_t *result = malloc((wcslen(page->summary) + 1) * sizeof(wchar_t));
        if (!result) {
            return NULL;
        }
        wcscpy(result, page->summary);
        return result;
    }

    // Fall back to first 240 characters of content (after stripping HTML)
    if (!page->content || wcslen(page->content) == 0) {
        wchar_t *result = malloc(sizeof(wchar_t));
        if (!result) {
            return NULL;
        }
        wcscpy(result, L"");
        return result;
    }

    // First strip HTML tags from the content
    wchar_t *stripped_content = strip_html_tags(page->content);
    if (!stripped_content) {
        return NULL;
    }

    size_t content_len = wcslen(stripped_content);
    size_t desc_len = content_len < 240 ? content_len : 240;

    wchar_t *result = malloc((desc_len + 1) * sizeof(wchar_t));
    if (!result) {
        free(stripped_content);
        return NULL;
    }

    wcsncpy(result, stripped_content, desc_len);
    result[desc_len] = L'\0';

    free(stripped_content);
    return result;
}