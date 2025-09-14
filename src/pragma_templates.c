/**
 * pragma_templates.c - Template system for structured HTML generation
 *
 * This module provides a structured template system that supports:
 * - Token replacement with {TOKEN} syntax
 * - Loop constructs for arrays (tags, posts, etc.)
 * - Conditional rendering based on data presence
 * - Template file loading from disk
 *
 * Templates use a simple syntax:
 * - {TOKEN} for simple replacement
 * - <!-- LOOP array_name --> ... <!-- END LOOP --> for arrays
 * - <!-- IF condition --> ... <!-- END IF --> for conditionals
 *
 * By Will Shaw <wsshaw@gmail.com>
 */

#include "pragma_poison.h"

/**
 * template_free(): Free all memory associated with a template_data structure.
 *
 * arguments:
 *  template_data *data (template data to free; may be NULL)
 *
 * returns:
 *  void
 */
void template_free(template_data *data) {
    if (!data) return;

    free(data->title);
    free(data->date);
    free(data->icon);
    free(data->content);
    free(data->prev_url);
    free(data->next_url);
    free(data->post_url);
    free(data->description);

    if (data->tags) {
        for (int i = 0; i < data->tag_count; i++) {
            free(data->tags[i]);
        }
        free(data->tags);
    }

    if (data->tag_urls) {
        for (int i = 0; i < data->tag_count; i++) {
            free(data->tag_urls[i]);
        }
        free(data->tag_urls);
    }

    free(data);
}

/**
 * template_data_from_page(): Create template data from a pragma page.
 *
 * Converts a pp_page structure into template_data for use with templates.
 * Handles tag parsing and URL generation.
 *
 * arguments:
 *  pp_page *page (page to convert; must not be NULL)
 *  site_info *site (site configuration; must not be NULL)
 *
 * returns:
 *  template_data* (heap-allocated template data; NULL on error)
 */
template_data* template_data_from_page(pp_page *page, site_info *site) {
    if (!page || !site) return NULL;

    template_data *data = malloc(sizeof(template_data));
    if (!data) return NULL;

    // Initialize all fields
    memset(data, 0, sizeof(template_data));

    // Basic page info
    data->title = page->title ? wcsdup(page->title) : NULL;
    data->icon = page->icon ? wcsdup(page->icon) : NULL;
    data->content = page->content ? wcsdup(page->content) : NULL;

    // Format date
    if (page->date_stamp > 0) {
        data->date = legible_date(page->date_stamp);
    }

    // Generate URLs (full URLs for proper og:url metadata)
    wchar_t *timestamp_str = string_from_int(page->date_stamp);
    if (timestamp_str) {
        size_t url_len = wcslen(site->base_url) + wcslen(timestamp_str) + 20;
        data->post_url = malloc(url_len * sizeof(wchar_t));
        if (data->post_url) {
            swprintf(data->post_url, url_len, L"%lsc/%ls.html", site->base_url, timestamp_str);
        }
        free(timestamp_str);
    }

    // Navigation URLs
    if (page->prev) {
        wchar_t *prev_id = string_from_int(page->prev->date_stamp);
        if (prev_id) {
            size_t url_len = wcslen(prev_id) + 10;
            data->prev_url = malloc(url_len * sizeof(wchar_t));
            if (data->prev_url) {
                swprintf(data->prev_url, url_len, L"%ls.html", prev_id);
            }
            free(prev_id);
        }
    }

    if (page->next) {
        wchar_t *next_id = string_from_int(page->next->date_stamp);
        if (next_id) {
            size_t url_len = wcslen(next_id) + 10;
            data->next_url = malloc(url_len * sizeof(wchar_t));
            if (data->next_url) {
                swprintf(data->next_url, url_len, L"%ls.html", next_id);
            }
            free(next_id);
        }
    }

    // Parse tags
    if (page->tags) {
        data->tag_count = 1; // At least one tag
        for (wchar_t *p = page->tags; *p; p++) {
            if (*p == L',') data->tag_count++;
        }

        data->tags = malloc(data->tag_count * sizeof(wchar_t*));
        data->tag_urls = malloc(data->tag_count * sizeof(wchar_t*));

        if (data->tags && data->tag_urls) {
            // Copy tags for tokenization
            wchar_t *tags_copy = wcsdup(page->tags);
            if (tags_copy) {
                wchar_t *token_state;
                wchar_t *tag = wcstok(tags_copy, L",", &token_state);
                int i = 0;

                while (tag && i < data->tag_count) {
                    // Trim whitespace
                    while (*tag == L' ' || *tag == L'\t') tag++;

                    data->tags[i] = wcsdup(tag);

                    // Generate tag URL
                    size_t tag_url_len = wcslen(tag) + 15;
                    data->tag_urls[i] = malloc(tag_url_len * sizeof(wchar_t));
                    if (data->tag_urls[i]) {
                        swprintf(data->tag_urls[i], tag_url_len, L"/t/%ls.html", tag);
                    }

                    i++;
                    tag = wcstok(NULL, L",", &token_state);
                }
                data->tag_count = i; // Actual count
                free(tags_copy);
            }
        }
    }

    // Get description
    data->description = get_page_description(page);

    // Set boolean flags
    data->has_prev = (data->prev_url != NULL);
    data->has_next = (data->next_url != NULL);
    data->has_navigation = data->has_prev || data->has_next;
    data->has_tags = (data->tag_count > 0);

    return data;
}

/**
 * load_template_file(): Load a template file from disk.
 *
 * Reads the entire template file into memory as a wide-character string.
 *
 * arguments:
 *  const char *template_path (path to template file; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated template content; NULL on error)
 */
wchar_t* load_template_file(const char *template_path) {
    if (!template_path) return NULL;

    FILE *file = fopen(template_path, "r");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    // Read file content
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    // Convert to wide characters
    wchar_t *wide_content = utf8_to_wchar(buffer);
    free(buffer);

    return wide_content;
}

/**
 * template_replace_token(): Replace all instances of a token in template.
 *
 * Replaces all occurrences of {token_name} with replacement_value.
 *
 * arguments:
 *  wchar_t *template (template string; must not be NULL)
 *  const wchar_t *token_name (token to replace, without braces; must not be NULL)
 *  const wchar_t *replacement_value (replacement text; may be NULL for empty)
 *
 * returns:
 *  wchar_t* (heap-allocated result; NULL on error)
 */
wchar_t* template_replace_token(wchar_t *template, const wchar_t *token_name, const wchar_t *replacement_value) {
    if (!template || !token_name) return NULL;
    if (!replacement_value) replacement_value = L"";

    // Build token pattern: {TOKEN_NAME}
    size_t token_len = wcslen(token_name) + 3; // {, }, \0
    wchar_t *token_pattern = malloc(token_len * sizeof(wchar_t));
    if (!token_pattern) return NULL;

    swprintf(token_pattern, token_len, L"{%ls}", token_name);

    // Use existing replace_substring function repeatedly until no more matches
    wchar_t *result = wcsdup(template);
    wchar_t *temp;

    while (result && wcsstr(result, token_pattern)) {
        temp = replace_substring(result, token_pattern, replacement_value);
        free(result);
        result = temp;
    }

    free(token_pattern);
    return result;
}

/**
 * template_process_loop(): Process a loop construct in template.
 *
 * Handles <!-- LOOP array_name --> ... <!-- END LOOP --> constructs.
 * Currently supports 'tags' array.
 *
 * arguments:
 *  wchar_t *template (template string; must not be NULL)
 *  template_data *data (template data; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated result; NULL on error)
 */
wchar_t* template_process_loop(wchar_t *template, template_data *data) {
    if (!template || !data) return NULL;

    // Look for <!-- LOOP tags --> ... <!-- END LOOP -->
    wchar_t *loop_start = wcsstr(template, L"<!-- LOOP tags -->");
    if (!loop_start) {
        return wcsdup(template); // No loops to process
    }

    wchar_t *loop_end = wcsstr(loop_start, L"<!-- END LOOP -->");
    if (!loop_end) {
        return wcsdup(template); // Malformed loop
    }

    // Calculate positions and lengths
    size_t before_loop_len = loop_start - template;
    wchar_t *loop_content_start = loop_start + wcslen(L"<!-- LOOP tags -->");
    size_t loop_content_len = loop_end - loop_content_start;
    wchar_t *after_loop_start = loop_end + wcslen(L"<!-- END LOOP -->");

    // Extract loop content
    wchar_t *loop_content = malloc((loop_content_len + 1) * sizeof(wchar_t));
    if (!loop_content) return NULL;

    wcsncpy(loop_content, loop_content_start, loop_content_len);
    loop_content[loop_content_len] = L'\0';

    // Build expanded loop content
    size_t expanded_size = 1; // Start with null terminator
    wchar_t *expanded_content = malloc(expanded_size * sizeof(wchar_t));
    if (!expanded_content) {
        free(loop_content);
        return NULL;
    }
    expanded_content[0] = L'\0';

    // Process each tag
    for (int i = 0; i < data->tag_count; i++) {
        // Replace {TAG} and {TAG_URL} in loop content
        wchar_t *item_content = template_replace_token(loop_content, L"TAG", data->tags[i]);
        if (item_content) {
            wchar_t *item_with_url = template_replace_token(item_content, L"TAG_URL", data->tag_urls[i]);
            free(item_content);

            if (item_with_url) {
                // Append to expanded content
                size_t comma_space_len = (i < data->tag_count - 1) ? 2 : 0; // ", " for non-last items
                size_t new_size = wcslen(expanded_content) + wcslen(item_with_url) + comma_space_len + 1;
                wchar_t *new_expanded = realloc(expanded_content, new_size * sizeof(wchar_t));
                if (new_expanded) {
                    expanded_content = new_expanded;
                    wcscat(expanded_content, item_with_url);
                    // Add comma and space after each tag except the last one
                    if (i < data->tag_count - 1) {
                        wcscat(expanded_content, L", ");
                    }
                }
                free(item_with_url);
            }
        }
    }

    // Assemble final result
    size_t result_size = before_loop_len + wcslen(expanded_content) + wcslen(after_loop_start) + 1;
    wchar_t *result = malloc(result_size * sizeof(wchar_t));

    if (result) {
        // Copy before loop
        wcsncpy(result, template, before_loop_len);
        result[before_loop_len] = L'\0';

        // Add expanded content
        wcscat(result, expanded_content);

        // Add after loop
        wcscat(result, after_loop_start);
    }

    free(loop_content);
    free(expanded_content);
    return result;
}

/**
 * template_process_conditionals(): Process conditional constructs in template.
 *
 * Handles <!-- IF condition --> ... <!-- END IF --> constructs.
 * Supports: has_navigation, has_tags, has_prev, has_next
 *
 * arguments:
 *  wchar_t *template (template string; must not be NULL)
 *  template_data *data (template data; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated result; NULL on error)
 */
wchar_t* template_process_conditionals(wchar_t *template, template_data *data) {
    if (!template || !data) return NULL;

    wchar_t *result = wcsdup(template);
    if (!result) return NULL;

    // Process each conditional type (process nested conditions multiple times)
    const wchar_t *conditionals[] = {
        L"has_navigation", L"has_tags", L"has_prev", L"has_next", NULL
    };

    // Process multiple times to handle nested conditions
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; conditionals[i]; i++) {
        // Build conditional markers
        size_t marker_len = wcslen(conditionals[i]) + 20;
        wchar_t *start_marker = malloc(marker_len * sizeof(wchar_t));
        wchar_t *end_marker = wcsdup(L"<!-- END IF -->");

        if (!start_marker || !end_marker) {
            free(start_marker);
            free(end_marker);
            continue;
        }

        swprintf(start_marker, marker_len, L"<!-- IF %ls -->", conditionals[i]);

        // Find conditional block
        wchar_t *cond_start = wcsstr(result, start_marker);
        if (cond_start) {
            wchar_t *cond_end = wcsstr(cond_start, end_marker);
            if (cond_end) {
                // Determine condition value
                bool condition_true = false;
                if (wcscmp(conditionals[i], L"has_navigation") == 0) {
                    condition_true = data->has_navigation;
                } else if (wcscmp(conditionals[i], L"has_tags") == 0) {
                    condition_true = data->has_tags;
                } else if (wcscmp(conditionals[i], L"has_prev") == 0) {
                    condition_true = data->has_prev;
                } else if (wcscmp(conditionals[i], L"has_next") == 0) {
                    condition_true = data->has_next;
                }

                // Replace conditional block
                size_t before_len = cond_start - result;
                wchar_t *after_start = cond_end + wcslen(end_marker);

                if (condition_true) {
                    // Keep content, remove markers
                    wchar_t *content_start = cond_start + wcslen(start_marker);
                    size_t content_len = cond_end - content_start;

                    // Build new result
                    size_t new_size = before_len + content_len + wcslen(after_start) + 1;
                    wchar_t *new_result = malloc(new_size * sizeof(wchar_t));
                    if (new_result) {
                        wcsncpy(new_result, result, before_len);
                        new_result[before_len] = L'\0';
                        wcsncat(new_result, content_start, content_len);
                        wcscat(new_result, after_start);

                        free(result);
                        result = new_result;
                    }
                } else {
                    // Remove entire conditional block
                    size_t new_size = before_len + wcslen(after_start) + 1;
                    wchar_t *new_result = malloc(new_size * sizeof(wchar_t));
                    if (new_result) {
                        wcsncpy(new_result, result, before_len);
                        new_result[before_len] = L'\0';
                        wcscat(new_result, after_start);

                        free(result);
                        result = new_result;
                    }
                }
            }
        }

        free(start_marker);
        free(end_marker);
        }
    }

    return result;
}

/**
 * apply_template(): Apply template data to a template string.
 *
 * Processes the complete template pipeline: loads template, replaces tokens,
 * processes loops and conditionals.
 *
 * arguments:
 *  const char *template_path (path to template file; must not be NULL)
 *  template_data *data (template data; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated rendered HTML; NULL on error)
 */
wchar_t* apply_template(const char *template_path, template_data *data) {
    if (!template_path || !data) return NULL;

    // Load template
    wchar_t *template = load_template_file(template_path);
    if (!template) return NULL;

    // Process loops first
    wchar_t *after_loops = template_process_loop(template, data);
    free(template);
    if (!after_loops) return NULL;

    // Process conditionals
    wchar_t *after_conditionals = template_process_conditionals(after_loops, data);
    free(after_loops);
    if (!after_conditionals) return NULL;

    // Replace basic tokens
    wchar_t *result = after_conditionals;

    const wchar_t *tokens[] = {
        L"TITLE", L"DATE", L"ICON", L"CONTENT", L"POST_URL",
        L"PREV_URL", L"NEXT_URL", L"DESCRIPTION", NULL
    };

    const wchar_t *values[] = {
        data->title, data->date, data->icon, data->content, data->post_url,
        data->prev_url, data->next_url, data->description
    };

    for (int i = 0; tokens[i]; i++) {
        wchar_t *temp = template_replace_token(result, tokens[i], values[i]);
        free(result);
        result = temp;
        if (!result) break;
    }

    return result;
}