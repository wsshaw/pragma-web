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

// State indicators for parsing inline/formatting elements that can span multiple lines
int bold = 0, italic = 0, within_unordered_list = 0, within_ordered_list = 0, block_quote = 0, code = 0, underline = 0, indent_level = 0;

// Safe buffer structure for dynamic string building with overflow protection
typedef struct {
    wchar_t *buffer;
    size_t size;
    size_t used;
} safe_buffer;

/**
 * safe_buffer_init(): Initialize a safe buffer with initial capacity.
 *
 * arguments:
 *  safe_buffer *buf (buffer to initialize; must not be NULL)
 *  size_t initial_size (initial capacity in wide characters)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_buffer_init(safe_buffer *buf, size_t initial_size) {
    if (!buf || initial_size == 0)
        return -1;
        
    buf->buffer = malloc(initial_size * sizeof(wchar_t));
    if (!buf->buffer)
        return -1;
        
    buf->size = initial_size;
    buf->used = 0;
    buf->buffer[0] = L'\0';
    return 0;
}

/**
 * safe_append(): Append text to a safe buffer with automatic reallocation.
 *
 * arguments:
 *  const wchar_t *text (text to append; must not be NULL)
 *  safe_buffer *buf (destination buffer; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_append(const wchar_t *text, safe_buffer *buf) {
    if (!text || !buf || !buf->buffer)
        return -1;
        
    size_t text_len = wcslen(text);
    if (buf->used + text_len + 1 >= buf->size) {
        // Reallocate with 50% more space
        size_t new_size = (buf->used + text_len + 1) * 3 / 2;
        wchar_t *new_buffer = realloc(buf->buffer, new_size * sizeof(wchar_t));
        if (!new_buffer)
            return -1;
            
        buf->buffer = new_buffer;
        buf->size = new_size;
    }
    
    wcscpy(buf->buffer + buf->used, text);
    buf->used += text_len;
    return 0;
}

/**
 * safe_buffer_free(): Free resources associated with a safe buffer.
 *
 * arguments:
 *  safe_buffer *buf (buffer to free; may be NULL)
 *
 * returns:
 *  void
 */
void safe_buffer_free(safe_buffer *buf) {
    if (buf && buf->buffer) {
        free(buf->buffer);
        buf->buffer = NULL;
        buf->size = 0;
        buf->used = 0;
    }
}

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

	// Build header HTML and append safely
	wchar_t header[wcslen(line) + 32]; // Extra space for tags
	swprintf(header, sizeof(header)/sizeof(wchar_t), L"<h%d>%ls</h%d>\n", level, line + level + 1, level);
	safe_append(header, output);
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
	if (output == NULL) { printf("Output is null \n"); }
	if (line == NULL) { printf("Line is null \n"); }
	safe_append(L"<p>", output);
	safe_append(line, output);
	safe_append(L"</p>\n", output);
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
	safe_append(L"<li>", output);
	safe_append(line + 2, output);
	safe_append(L"</li>\n", output);
}

/**
 * md_empty_line(): Handle an empty Markdown line.
 *
 * Currently appends a HTML line break "<br>\n". Future revisions may
 * collapse consecutive breaks or manage paragraph spacing a little better.
 *
 * arguments:
 *  wchar_t *output (destination buffer for appended HTML; must not be NULL)
 *
 * returns:
 *  void
 */
void md_empty_line(safe_buffer *output) {
	safe_append(L"<br>\n", output);
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
		wprintf(L"Invalid input or output parameters.\n");
			return;
	}

	size_t original_length = wcslen(original);
	output_size = output_size > original_length ? original_length * 2 : output_size - 1;

	if (output_size <= original_length) {
		wprintf(L"! insufficient output buffer in md_escape()! will not escape markdown\n");
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
 *  wchar_t *output   (destination buffer for appended HTML; must not be NULL)
 *
 * returns:
 *  void
 */
void md_inline(wchar_t *original, safe_buffer *output) {
	size_t length = wcslen(original);
	for (size_t i = 0; i < length; i++) {
		if (original[i] == L'*' && (i + 1 < length)) {
			if (original[i + 1] == L'*') { // bold
				safe_append( bold ? L"</strong>" : L"<strong>", output );
				bold = 1 - bold;
				i++;
			} else {
				safe_append( italic ? L"</i>" : L"<i>", output );
				italic = 1 - italic;
			}	
		} else if (original[i] == L'`') { // code
			safe_append( code ? L"</code>" : L"<code>", output );
			code = 1 - code;
		} else if (original[i] == L'_') { // underline
			safe_append( underline ? L"</u>" : L"<u>", output);
			underline = 1 - underline;
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
				while (i < length && (original[i + 1] != L')' && original[i + 1] != L' ')) 
					i++;
				
				// Check bounds for URL
				if (i >= length) {
					free(alt_text);
					wchar_t single_char[2] = {L'!', L'\0'};
					safe_append(single_char, output);
					continue;
				}
				
				end = i + 1;
				size_t img_url_length = end - start;

				// Safely allocate memory for image URL
				wchar_t *image_url = malloc((img_url_length + 1) * sizeof(wchar_t));
				if (!image_url) {
					free(alt_text);
					wchar_t single_char[2] = {original[i], L'\0'};
					safe_append(single_char, output);
					continue;
				}
				wcsncpy(image_url, original + start, img_url_length);
				image_url[img_url_length] = L'\0';

				// tk support for caption 

				// Construct the <img> tag
				safe_append(L"<img class=\"post\" src=\"", output);
				safe_append(image_url, output);
				safe_append(L"\" alt=\"", output);
				safe_append(alt_text, output);
				safe_append(L"\">", output);
				
				// Free allocated memory
				free(alt_text);
				free(image_url);
				++i;
			} else if (i + 1 < length && original[i + 1] == L'!') {
				// gallery tk
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

	// placemarkers while parsing out the line
  	wchar_t *s, *e;
	s = e = input;
  	s = e = (wchar_t*) input; 

	// ensure that state values are reset when starting a new file
	bold = italic = within_unordered_list = block_quote = code = underline = 0;

	// Use this method instead of wcstok() because we need to keep the newlines
  	while((e = wcschr(s, L'\n'))) {

		// figure out how much space we need for this line
		int line_length = (int)(e - s + 1);
		wchar_t *line = malloc((line_length + 1) * sizeof(wchar_t)); 	

		if (line == NULL) { 
			printf("malloc() failed to allocate space for parsing a line in parse_markdown!"); 
			break;
		}
	
		// get the line as displayed by printf()
		swprintf(line, line_length + 1, L"%.*ls", line_length, s);
	
		// allocate memory for escaping literals (worst case: every char escaped = 2x length)
		size_t esc_size = (wcslen(line) * 2 + 1);
		wchar_t *esc = malloc(esc_size * sizeof(wchar_t));
		if (esc == NULL) { printf("malloc() failed in parsing markdown!\n"); }
		md_escape(line, esc, esc_size);

		// allocate buffer for inline formatting (estimate 3x for HTML tags)
		size_t fmt_size = wcslen(line) * 3 + 256;
		safe_buffer fmt;
		if (safe_buffer_init(&fmt, fmt_size) != 0) {
			printf("failed to init buffer for inline formatting!\n");
			free(esc);
			free(line);
			continue;
		}
		md_inline(esc, &fmt);

		if (fmt.buffer[0] == L'#') {
			// a line starting with # must be a heading
			md_header(fmt.buffer, &output);
		} else if (fmt.buffer[0] == L'-' && fmt.buffer[1] == L' ') {
			// a line beginning with "- " is an unordered list item		
			// close any previous lists (nesting = \t, not lists of lists)
			if (within_ordered_list) {
				within_ordered_list = 0;
				safe_append(L"</ol>\n", &output);
			}
			if (!within_unordered_list) {
				within_unordered_list = 1;
				safe_append(L"<ul>\n", &output); 
			}
			md_list(fmt.buffer, &output);
		} else if (iswdigit(fmt.buffer[0]) && fmt.buffer[1] == L'.') {
			// a line beginning with "1." (or "\d\." in general) is an ordered list item
			if (within_unordered_list) {
				within_unordered_list = 0;
				safe_append(L"</ul>\n", &output);
			}
			if (!within_ordered_list) {
				within_ordered_list = 1;
				safe_append(L"<ol>\n", &output);
			}
			md_list(fmt.buffer, &output);
		} else if (fmt.buffer[0] == L'>') {
			if (!block_quote) {
				block_quote = 1;
				safe_append(L"<blockquote>", &output);
			}
			md_paragraph(fmt.buffer + 1, &output);
		} else if ((fmt.buffer[0] == '\0') || (fmt.buffer[0] == '\n')) {
			md_empty_line(&output);
		} else {
			if (within_unordered_list) {
				within_unordered_list = 0;
				safe_append(L"</ul>", &output);
			} 
			if (within_ordered_list) {
				within_ordered_list = 0;
				safe_append(L"</ol>\n", &output);
			}
			if (block_quote) { 
				block_quote = 0;
				safe_append(L"</blockquote>", &output);
			}
			md_paragraph(fmt.buffer, &output);
		}
		free(esc);
		safe_buffer_free(&fmt);
		free(line);
		s = e + 1;
	}		

	// Clean up: did we leave a list open?  Bold?  etc.
	if (within_unordered_list) {
		within_unordered_list = 0;
		safe_append(L"</ul>\n", &output);
	} 
	if (within_ordered_list) {
		within_ordered_list = 0;
		safe_append(L"</ol>\n", &output);
	}
	if (bold) 
		safe_append(L"</strong>", &output);
	if (italic) 
		safe_append(L"</i>", &output);
	if (block_quote)
		safe_append(L"</blockquote>", &output);
	if (code)
		safe_append(L"</code>", &output);
	if (underline)
		safe_append(L"</u>", &output);
	
	// Return the buffer, caller is responsible for freeing
	wchar_t *result = output.buffer;
	// Don't free the buffer since we're returning it
	output.buffer = NULL;
	safe_buffer_free(&output);
	return result;		  
}
