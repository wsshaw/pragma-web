/**
* pragma_markdown - markdown parsing functions for the pragma web generator.
*
* The markdown parser (in progress) is an ad hoc, self-contined minimalist parser that is meant to 
* provide support for a few Markdown elements:
*
* - Headings (#, ##, etc) 
* - Paragraphs (two successive line breaks)
* - Horizontal rule (---, ***, or ___ between blank lines)
* - Bold (**) and italic (*), plus a combination (***) of the two
* - Links ([Link Title](url), or [Link Title](url "Optional hover text"))
* - Lists (1. x\n1. y\n1. z ... or - x\n- y\n ...), with nesting via indentation
* - Backticks for code, `like this`
* - Blockquotes (>)
* - Images (![Alt text](url))
* - Underlining (_text_)
* - Line breaks (two or more spaces at the end of a line followed by \n, but not a "\" followed by \n)
* - Image galleries (!!(directory)) e.g. !!(/fido/temp) 
*
* HTML can be included alongside the markdown.  
*
* Any page source containing parse:false in the header will not be processed by the markdown parser.
*
* By Will Shaw <wsshaw@gmail.com>
*/

#include "pragma_poison.h"

// Parser state structure for thread-safe parsing
typedef struct {
    int bold;
    int italic;
    int within_unordered_list;
    int within_ordered_list;
    int block_quote;
    int code;
    int underline;
    int indent_level;
} md_parser_state;


/*
* md_header(): Convert a Markdown heading line (#, ##, …, ######) into an HTML <hN>.
*
* Determines heading level by counting leading # (up to 6) and appends a
* corresponding <hN> element to the output buffer.
*
* arguments:
*  wchar_t *line   (input line beginning with one or more '#' characters; must not be NULL)
*  wchar_t *output (destination buffer for appended HTML; must not be NULL)
*
* returns:
*  void
*/
void md_header(wchar_t *line, safe_buffer *output) {
	int level = 0;

	// Determine which header element to use (1-6)
	while (line[level] == L'#' && level < 6)
		level++;

	// Build header HTML using the new HTML element function
	wchar_t *heading = html_heading(level, line + level + 1, NULL, false);
	if (heading) {
		safe_append(heading, output);
		safe_append(L"\n", output);
		free(heading);
	}
}

/**
 * md_paragraph(): Wrap a line of text in a paragraph element.
 *
 * Appends "<p>" + line + "</p>\n" to the output buffer. Caller ensures the
 * provided line has already been escaped/formatted as needed.
 *
 * arguments:
 *  wchar_t *line   (text to include inside the paragraph; must not be NULL)
 *  wchar_t *output (destination buffer for appended HTML; must not be NULL)
 *
 * returns:
 *  void
 */
void md_paragraph(wchar_t *line, safe_buffer *output) {
	if (PRAGMA_DEBUG) {
		if (output == NULL) { 
				log_debug("Output is null \n"); 
		}
		if (line == NULL) { 
				log_debug("Line is null \n"); 
		}
	}

	wchar_t *paragraph = html_paragraph(line, NULL, false);
	if (paragraph) {
		safe_append(paragraph, output);
		safe_append(L"\n", output);
		free(paragraph);
	}
}

/**
 * md_list(): Append a single list item (<li>) derived from a Markdown list line.
 *
 * For unordered lines expected to begin with "- " (or ordered with "N." pre-trimmed),
 * appends "<li>...</li>\n" to the output buffer.
 *
 * arguments:
 *  wchar_t *line   (the source line; for "- " lists, text begins at line + 2)
 *  wchar_t *output (destination buffer for appended HTML; must not be NULL)
 *
 * returns:
 *  void
 */
void md_list(wchar_t *line, safe_buffer *output) {
	wchar_t *list_item = html_list_item(line + 2, NULL, false);
	if (list_item) {
		safe_append(list_item, output);
		safe_append(L"\n", output);
		free(list_item);
	}
}

/**
 * md_empty_line(): Handle an empty Markdown line.
 *
 * In proper Markdown, blank lines separate paragraphs and don't create <br> tags.
 * This function now does nothing, allowing natural paragraph separation.
 *
 * arguments:
 *  safe_buffer *output (destination buffer; may be NULL, unused)
 *
 * returns:
 *  void
 */
void md_empty_line(safe_buffer *output) {
	// Blank lines in Markdown separate paragraphs, not create line breaks
	// Do nothing - let natural paragraph spacing handle this
	(void)output; // Suppress unused parameter warning
}

/**
 * md_is_horizontal_rule(): Check if a line is a horizontal rule.
 *
 * Horizontal rules are lines containing only 3+ dashes (---), asterisks (***),
 * or underscores (___), optionally with whitespace.
 *
 * arguments:
 *  wchar_t *line (line to check; must not be NULL)
 *
 * returns:
 *  bool (true if line is a horizontal rule, false otherwise)
 */
bool md_is_horizontal_rule(wchar_t *line) {
    if (!line) return false;

    wchar_t rule_char = 0;
    int count = 0;

    for (size_t i = 0; line[i] != L'\0'; i++) {
        if (line[i] == L'-' || line[i] == L'*' || line[i] == L'_') {
            if (rule_char == 0) {
                rule_char = line[i]; // Set the rule character
            } else if (rule_char != line[i]) {
                return false; // Mixed characters, not a rule
            }
            count++;
        } else if (line[i] != L' ' && line[i] != L'\t' && line[i] != L'\n') {
            return false; // Non-whitespace character, not a rule
        }
    }

    return count >= 3; // Need at least 3 characters
}

/**
 * md_horizontal_rule(): Append a horizontal rule (<hr>) to output.
 *
 * arguments:
 *  safe_buffer *output (destination buffer for appended HTML; must not be NULL)
 *
 * returns:
 *  void
 */
void md_horizontal_rule(safe_buffer *output) {
    safe_append(L"<hr>\n", output);
}

/**
 * md_escape(): Process backslash escapes in a Markdown line.
 *
 * Copies `original` into `output` while honoring backslash escapes (e.g., "\\*" yields "*").
 * Validates output buffer capacity and terminates with L'\0'. This function does NOT perform
 * HTML entity escaping; it only interprets Markdown-style backslash escapes.
 *
 * arguments:
 *  const wchar_t *original (source text; must not be NULL)
 *  wchar_t       *output   (destination buffer; must not be NULL)
 *  size_t         output_size (size of destination buffer, in wide characters)
 *
 * returns:
 *  void
 */
void md_escape(const wchar_t *original, wchar_t *output, size_t output_size) {
	if (original == NULL || output == NULL || output_size == 0) {
		// need to add error handling for null pointers or invalid output size.
		// also don't lump all those conditions together
		log_warn("Invalid input or output parameters in md_escape()");
		return;
	}

	size_t original_length = wcslen(original);
	output_size = output_size > original_length ? original_length * 2 : output_size - 1;

	if (output_size <= original_length) {
		log_error("insufficient output buffer in md_escape()! will not escape markdown.\n");
		return;
	}

	size_t j = 0;
	for (size_t i = 0; i < original_length && j < output_size - 1; i++) {
		if (original[i] == L'\\' && i + 1 < original_length)
			output[j++] = original[++i]; // next must be a literal
		else 
			output[j++] = original[i];
	}
	output[j] = L'\0';
}

/**
 * md_inline(): Parse and render inline Markdown formatting tokens.
 *
 * Supports **bold**, *italic*, `code`, _underline_, images ![alt](url),
 * and toggles stateful spans across lines where applicable. Appends HTML
 * to the output buffer and ensures NUL termination.
 *
 * arguments:
 *  wchar_t *original (input text containing inline Markdown; must not be NULL)
 *  safe_buffer *output (destination buffer for appended HTML; must not be NULL)
 *  md_parser_state *state (parser state for tracking formatting; must not be NULL)
 *
 * returns:
 *  void
 */
void md_inline(wchar_t *original, safe_buffer *output, md_parser_state *state) {
	size_t length = wcslen(original);
	for (size_t i = 0; i < length; i++) {
		if (original[i] == L'*' && (i + 1 < length)) {
			if (original[i + 1] == L'*') { // bold
				safe_append( state->bold ? L"</strong>" : L"<strong>", output );
				state->bold = 1 - state->bold;
				i++;
			} else {
				safe_append( state->italic ? L"</i>" : L"<i>", output );
				state->italic = 1 - state->italic;
			}	
		} else if (original[i] == L'`') { // code
			safe_append( state->code ? L"</code>" : L"<code>", output );
			state->code = 1 - state->code;
		} else if (original[i] == L'_') { // underline
			safe_append( state->underline ? L"</u>" : L"<u>", output);
			state->underline = 1 - state->underline;
		} else if (original[i] == L'[') { // links [text](url)
			// Find out bounds of the link text first...
			size_t start = i + 1;
			while (i + 1 < length && original[i + 1] != L']')
				i++;

			// Check bounds and calculate proper length
			if (i + 1 >= length) {
				// Malformed link syntax, treat as literal character
				wchar_t single_char[2] = {original[i], L'\0'};
				safe_append(single_char, output);
				continue;
			}

			size_t end = i + 1;
			size_t link_text_length = end - start;

			// Safely allocate memory for link text
			wchar_t *link_text = malloc((link_text_length + 1) * sizeof(wchar_t));
			if (!link_text) {
				// Memory allocation failed, treat as literal
				wchar_t single_char[2] = {original[i], L'\0'};
				safe_append(single_char, output);
				continue;
			}
			wcsncpy(link_text, original + start, link_text_length);
			link_text[link_text_length] = L'\0';

			// Skip '](' - check if we have '(' after ']'
			if (i + 2 >= length || original[i + 2] != L'(') {
				free(link_text);
				wchar_t single_char[2] = {L'[', L'\0'};
				safe_append(single_char, output);
				continue;
			}
			i += 3;

			// Find out bounds of the link url element
			start = i;
			while (i < length && original[i] != L')' && original[i] != L' ')
				i++;

			// Check bounds for URL
			if (i >= length || original[i] != L')') {
				free(link_text);
				wchar_t single_char[2] = {L'[', L'\0'};
				safe_append(single_char, output);
				continue;
			}

			end = i;
			size_t link_url_length = end - start;

			// Safely allocate memory for link URL
			wchar_t *link_url = malloc((link_url_length + 1) * sizeof(wchar_t));
			if (!link_url) {
				free(link_text);
				wchar_t single_char[2] = {original[i], L'\0'};
				safe_append(single_char, output);
				continue;
			}
			wcsncpy(link_url, original + start, link_url_length);
			link_url[link_url_length] = L'\0';

			// Construct the <a> tag using the HTML function
			wchar_t *link_tag = html_link(link_url, link_text, NULL, false);
			if (link_tag) {
				safe_append(link_tag, output);
				free(link_tag);
			}

			// Free allocated memory
			free(link_text);
			free(link_url);
			// i is already at the closing ')' so no need to increment
		} else if (original[i] == L'!') { // images 
			if (i + 1 < length && original[i + 1] == L'[') {
				// Find out bounds of the alt text element first...
				size_t start = i + 2; 
				while (i + 1 < length && original[i + 1] != L']') 
					i++;
				
				// Check bounds and calculate proper length
				if (i + 1 >= length) {
					// Malformed image syntax, treat as literal character
					wchar_t single_char[2] = {original[i], L'\0'};
					safe_append(single_char, output);
					continue;
				}
				
				size_t end = i + 1;
				size_t img_alt_length = end - start;

				// Safely allocate memory for alt text
				wchar_t *alt_text = malloc((img_alt_length + 1) * sizeof(wchar_t));
				if (!alt_text) {
					// Memory allocation failed, treat as literal
					wchar_t single_char[2] = {original[i], L'\0'};
					safe_append(single_char, output);
					continue;
				}
				wcsncpy(alt_text, original + start, img_alt_length);
				alt_text[img_alt_length] = L'\0';

				// Skip ']('
				i += 3;

				// Find out bounds of the image url element
				start = i;
				while (i < length && original[i] != L')' && original[i] != L' ' && original[i] != L'"')
					i++;

				// Check bounds for URL
				if (i >= length) {
					free(alt_text);
					wchar_t single_char[2] = {L'!', L'\0'};
					safe_append(single_char, output);
					continue;
				}

				// Parse URL and optional caption: ![alt](url "caption")
				wchar_t *image_url = NULL;
				wchar_t *caption = NULL;

				// Look for space to separate URL from caption
				size_t space_pos = start;
				while (space_pos < length && original[space_pos] != L' ' && original[space_pos] != L')')
					space_pos++;

				if (space_pos < length && original[space_pos] == L' ') {
					// Found space - URL ends here, caption may follow
					size_t url_length = space_pos - start;
					image_url = malloc((url_length + 1) * sizeof(wchar_t));
					if (image_url) {
						wcsncpy(image_url, original + start, url_length);
						image_url[url_length] = L'\0';
					}

					// Skip whitespace after URL
					i = space_pos;
					while (i < length && original[i] == L' ') i++;

					// Look for caption in quotes
					if (i < length && original[i] == L'"') {
						i++; // Skip opening quote
						size_t caption_start = i;

						// Find closing quote
						while (i < length && original[i] != L'"')
							i++;

						if (i < length && original[i] == L'"') {
							size_t caption_length = i - caption_start;
							caption = malloc((caption_length + 1) * sizeof(wchar_t));
							if (caption) {
								wcsncpy(caption, original + caption_start, caption_length);
								caption[caption_length] = L'\0';
							}
							i++; // Skip closing quote
						}
					}
				} else {
					// No space found - URL extends to closing paren or current position
					end = (space_pos < length && original[space_pos] == L')') ? space_pos : i;
					size_t url_length = end - start;
					image_url = malloc((url_length + 1) * sizeof(wchar_t));
					if (image_url) {
						wcsncpy(image_url, original + start, url_length);
						image_url[url_length] = L'\0';
					}
					i = end;
				}

				// Find closing parenthesis
				while (i < length && original[i] != L')')
					i++;

				if (i >= length) {
					free(alt_text);
					free(image_url);
					free(caption);
					wchar_t single_char[2] = {L'!', L'\0'};
					safe_append(single_char, output);
					continue;
				} 

				// Construct the image tag, using caption if available
				wchar_t *image_tag = html_image_with_caption(image_url, alt_text, caption, L"post");
				if (image_tag) {
					safe_append(image_tag, output);
					free(image_tag);
				}

				// Free allocated memory
				free(alt_text);
				free(image_url);
				free(caption);
				++i;
			} else if (i + 1 < length && original[i + 1] == L'!') {
				// Parse image gallery: !!(directory)
				if (i + 2 < length && original[i + 2] == L'(') {
					i += 3; // Skip "!!("

					// Find directory path
					size_t start = i;
					while (i < length && original[i] != L')')
						i++;

					if (i >= length) {
						// Missing closing parenthesis, treat as literal
						wchar_t single_char[2] = {L'!', L'\0'};
						safe_append(single_char, output);
						continue;
					}

					size_t end = i;
					size_t dir_path_length = end - start;

					// Allocate memory for directory path
					wchar_t *dir_path = malloc((dir_path_length + 1) * sizeof(wchar_t));
					if (!dir_path) {
						// Memory allocation failed, treat as literal
						wchar_t single_char[2] = {L'!', L'\0'};
						safe_append(single_char, output);
						continue;
					}
					wcsncpy(dir_path, original + start, dir_path_length);
					dir_path[dir_path_length] = L'\0';

					// Generate gallery HTML
					wchar_t *gallery_html = html_image_gallery(dir_path, L"gallery");
					if (gallery_html) {
						safe_append(gallery_html, output);
						free(gallery_html);
					}

					free(dir_path);
					++i; // Skip closing parenthesis
				} else {
					// Not a gallery, treat as literal
					wchar_t single_char[2] = {L'!', L'\0'};
					safe_append(single_char, output);
				}
			} else {
				wchar_t single_char[2] = {original[i], L'\0'};
				safe_append(single_char, output);
			}
		} else { // Doesn't seem to be an inline/formatting thing; continue
			wchar_t single_char[2] = {original[i], L'\0'};
			safe_append(single_char, output);
		}
	} // end for loop
}

/**
 * parse_markdown(): Main entry point for converting Markdown to HTML.
 *
 * Processes a wide-character input buffer line-by-line, handling headings,
 * lists (ordered/unordered), block quotes, paragraphs, empty lines, and
 * inline formatting. Resets parser state at start. Returns a newly allocated
 * wide-character buffer containing HTML; caller is responsible for free().
 *
 * arguments:
 *  wchar_t *input (entire Markdown document as a single wide string; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated HTML output buffer; caller must free)
 */
wchar_t* parse_markdown(wchar_t *input) {
	if (!input)
		return NULL;

	// Initialize safe buffer with reasonable initial capacity
	safe_buffer output;
	if (safe_buffer_init(&output, wcslen(input) + 1024) != 0) {
		return NULL;
	}

	// Initialize parser state
	md_parser_state state = {0};

	// placemarkers while parsing out the line
  	wchar_t *s, *e;
	s = e = input;
  	s = e = (wchar_t*) input;

	// Get reusable buffers from the global pool for better memory efficiency
	safe_buffer *line_buf = buffer_pool_get_global();
	safe_buffer *esc_buf = buffer_pool_get_global();
	safe_buffer *fmt_buf = buffer_pool_get_global();

	if (!line_buf || !esc_buf || !fmt_buf) {
		if (line_buf) buffer_pool_return_global(line_buf);
		if (esc_buf) buffer_pool_return_global(esc_buf);
		if (fmt_buf) buffer_pool_return_global(fmt_buf);
		safe_buffer_free(&output);
		return NULL;
	}

	// Use this method instead of wcstok() because we need to keep the newlines
  	while((e = wcschr(s, L'\n'))) {
		// Reset and prepare buffers for this line
		safe_buffer_reset(line_buf);
		safe_buffer_reset(esc_buf);
		safe_buffer_reset(fmt_buf);

		// Extract line without additional malloc
		int line_length = (int)(e - s);
		if (line_length >= 0) {
			// Copy line directly to buffer
			if (safe_append_char(L'\0', line_buf) != 0) { // ensure null termination space
				buffer_pool_return_global(line_buf);
				buffer_pool_return_global(esc_buf);
				buffer_pool_return_global(fmt_buf);
				safe_buffer_free(&output);
				return NULL;
			}
			safe_buffer_reset(line_buf); // reset after ensuring capacity

			for (int i = 0; i < line_length; i++) {
				if (safe_append_char(s[i], line_buf) != 0) {
					buffer_pool_return_global(line_buf);
					buffer_pool_return_global(esc_buf);
					buffer_pool_return_global(fmt_buf);
					safe_buffer_free(&output);
					return NULL;
				}
			}
			if (safe_append_char(L'\n', line_buf) != 0 || safe_append_char(L'\0', line_buf) != 0) {
				buffer_pool_return_global(line_buf);
				buffer_pool_return_global(esc_buf);
				buffer_pool_return_global(fmt_buf);
				safe_buffer_free(&output);
				return NULL;
			}
		}

		// Escape using buffer (more efficient than malloc)
		md_escape(line_buf->buffer, esc_buf->buffer, esc_buf->size);

		// Process inline formatting using the buffer directly
		md_inline(esc_buf->buffer, fmt_buf, &state);

		if (fmt_buf->buffer[0] == L'#') {
			// a line starting with # must be a heading
			md_header(fmt_buf->buffer, &output);
		} else if (fmt_buf->buffer[0] == L'-' && fmt_buf->buffer[1] == L' ') {
			// a line beginning with "- " is an unordered list item		
			// close any previous lists (nesting = \t, not lists of lists)
			if (state.within_ordered_list) {
				state.within_ordered_list = 0;
				safe_append(L"</ol>\n", &output);
			}
			if (!state.within_unordered_list) {
				state.within_unordered_list = 1;
				safe_append(L"<ul>\n", &output); 
			}
			md_list(fmt_buf->buffer, &output);
		} else if (iswdigit(fmt_buf->buffer[0]) && fmt_buf->buffer[1] == L'.') {
			// a line beginning with "1." (or "\d\." in general) is an ordered list item
			if (state.within_unordered_list) {
				state.within_unordered_list = 0;
				safe_append(L"</ul>\n", &output);
			}
			if (!state.within_ordered_list) {
				state.within_ordered_list = 1;
				safe_append(L"<ol>\n", &output);
			}
			md_list(fmt_buf->buffer, &output);
		} else if (fmt_buf->buffer[0] == L'>') {
			if (!state.block_quote) {
				state.block_quote = 1;
				safe_append(L"<blockquote>", &output);
			}
			md_paragraph(fmt_buf->buffer + 1, &output);
		} else if (md_is_horizontal_rule(fmt_buf->buffer)) {
			// Horizontal rule: ---, ***, or ___
			md_horizontal_rule(&output);
		} else if ((fmt_buf->buffer[0] == '\0') || (fmt_buf->buffer[0] == '\n')) {
			md_empty_line(&output);
		} else {
			if (state.within_unordered_list) {
				state.within_unordered_list = 0;
				safe_append(L"</ul>", &output);
			} 
			if (state.within_ordered_list) {
				state.within_ordered_list = 0;
				safe_append(L"</ol>\n", &output);
			}
			if (state.block_quote) { 
				state.block_quote = 0;
				safe_append(L"</blockquote>", &output);
			}
			md_paragraph(fmt_buf->buffer, &output);
		}
		s = e + 1;
	}

	// Return buffers to pool
	buffer_pool_return_global(line_buf);
	buffer_pool_return_global(esc_buf);
	buffer_pool_return_global(fmt_buf);		

	// Clean up: did we leave a list open?  Bold?  etc.
	if (state.within_unordered_list) {
		state.within_unordered_list = 0;
		safe_append(L"</ul>\n", &output);
	} 
	if (state.within_ordered_list) {
		state.within_ordered_list = 0;
		safe_append(L"</ol>\n", &output);
	}
	if (state.bold) 
		safe_append(L"</strong>", &output);
	if (state.italic) 
		safe_append(L"</i>", &output);
	if (state.block_quote)
		safe_append(L"</blockquote>", &output);
	if (state.code)
		safe_append(L"</code>", &output);
	if (state.underline)
		safe_append(L"</u>", &output);
	
	// Return the buffer, caller is responsible for freeing
	wchar_t *result = output.buffer;
	// Don't free the buffer since we're returning it
	output.buffer = NULL;
	safe_buffer_free(&output);
	return result;		  
}
