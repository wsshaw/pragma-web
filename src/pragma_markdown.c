/**
* pragma_markdown - markdown parsing functions for the pragma poison generator.
* (c) 2023 Will Shaw <will@pragmapoison.org> 
* 
* Made available under the MIT License
*
* The markdown parser is more "ad hoc" and "functional" than it is "robust" and "well-designed," but it works.
* It supports the following Markdown elements:
*
* - Headings (#, ##, etc)
* - Paragraphs (two successive line breaks)
* - Line breaks (two or more spaces at the end of a line followed by \n, but not a "\" followed by \n)
* - Horizontal rule (---, ***, or ___ between blank lines)
* - Bold (**) and italic (*), plus a combination (***) of the two
* - Underlining (_text_)
* - Links ([Link Title](url), or [Link Title](url "Optional hover text"))
* - Lists (1. x\n 2. y\n ... or - x\n- y\n ...), with nesting via indentation
* - Backticks for code, `like this`
* - Blockquotes (>)
* - Images (![Alt text](url))
*
* HTML can be included alongside the markdown.  
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <malloc/malloc.h>
#include <wchar.h>

#include "pragma_poison.h"

// State indicators for parsing inline/formatting elements that can span multiple lines
int bold = 0, italic = 0, within_list = 0, block_quote = 0;

void md_header(wchar_t *line, wchar_t *output) {
	int level = 0; 

	// Determine which header element to use (1-6)
	while (line[level] == L'#' && level < 6) 
		level++;

	// Hard-coded length here (line + 9) is meant to reflect the max # of wide characters added to the line
	// e.g. <h1></h1> = 9
	swprintf(output + wcslen(output), wcslen(line) + 9, L"<h%d>%ls</h%d>\n", level, line + level + 1, level);
}

void md_paragraph(wchar_t *line, wchar_t *output) {
	if (output == NULL) { printf("Output is null \n"); }
	if (line == NULL) { printf("Line is null \n"); }
	append(L"<p>", output, NULL);
	append(line, output, NULL);
	append(L"</p>\n", output, NULL);
}

void md_list(wchar_t *line, wchar_t *output) {
	append(L"<li>", output, NULL);
	append(line + 2, output, NULL);
	append(L"</li>\n", output, NULL);
}

void md_empty_line(wchar_t *output) {
	append(L"<br>\n", output, NULL);
}

void md_escape(const wchar_t *original, wchar_t *output, size_t output_size) {
	if (original == NULL || output == NULL || output_size == 0) {
		// Add error handling for null pointers or invalid output size
		wprintf(L"Invalid input or output parameters.\n");
			return;
	}

	size_t original_length = wcslen(original);

	output_size = output_size > original_length ? original_length * 2 : output_size - 1;

	if (output_size <= original_length) {
			wprintf(L"Output buffer size is too small.\n");
		return;
	}

	size_t j = 0;

	for (size_t i = 0; i < original_length && j < output_size - 1; i++) {
		if (original[i] == L'\\' && i + 1 < original_length)
			output[j++] = original[++i]; // next is literal
		else 
			output[j++] = original[i];
		}
	output[j] = L'\0';
}

// Parse inline elements such as **, *, etc.
void md_inline(wchar_t *original, wchar_t *output) {
	size_t j = 0;
	size_t length = wcslen(original);
	for (size_t i = 0; i < length; i++) {
		if (original[i] == L'*' && (i + 1 < length)) {
			if (original[i + 1] == L'*') { // bold
				append( bold ? L"</em>" : L"<em>", output, &j );
				bold = 1 - bold;
				i++;
			} else {
				append( italic ? L"</i>" : L"<i>", output, &j );
				italic = 1 - italic;
			}	
		} else if (original[i] == L'!') { // images 
			if (i + 1 < length && original[i + 1] == L'[') {
				// Find out bounds of the image element
				size_t start = i + 2; 

				while (i + 1 < length && original[i + 1] != L']') 
					i++;

				size_t end = i;
				size_t img_alt_length = end - start + 1;

				// Isolate alt text 
				wchar_t alt_text[img_alt_length + 1];
				wcsncpy(alt_text, original + start, img_alt_length); // was -1
				alt_text[img_alt_length] = L'\0';

				// Skip ']('
				i += 3;

				// Find out bounds of the alt text element
				start = i;
				while (i + 1 < length && (original[i + 1] != L')' && original[i+1] != L' ')) i++;
				end = i;

				size_t img_url_length = end - start + 1;
				wchar_t image_url[img_url_length + 1];

				wcsncpy(image_url, original + start, img_url_length); // was length - 1
				image_url[img_url_length] = L'\0';

				// tk support for caption 

				// Construct the <img> tag
				append(L"<img src=\"", output, &j);
				append(image_url, output, &j);
				append(L"\" alt=\"", output, &j);
				append(alt_text, output, &j);
				append(L"\">", output, &j);
			} else {
				output[j++] = original[i];
			}
		} else { // Doesn't seem to be an inline/formatting thing; continue
			output[j++] = original[i];
		}
	} // end for loop

	// wrap up the string
	output[j] = L'\0';
}

wchar_t* parse_markdown(wchar_t *input) {
	wchar_t *output = malloc(wcslen(input) * 8); // lol what
	output[0] = L'\0';

  	wchar_t *s, *e;
	s = e = input;
  	s = e = (wchar_t*) input; 
	//wprintf(L"%ls", input);

	// ensure that values are reset to 0 when starting a new file
	bold = italic = within_list = 0;

	// Use this instead of strtok() (or whatever its 'wide' equivalent is) because we need to keep the newlines
  	while((e = wcschr(s, L'\n'))) {
		int line_length = (int)(e - s + 1);
		wchar_t *line = malloc((line_length + 1) * sizeof(wchar_t)); 	

		if (line == NULL) { printf("malloc() failed to allocate space for parsing a line in parse_markdown!"); }

		// Get the line as parsed by strchr()
		swprintf(line, line_length + 1, L"%.*ls", line_length, s);
	
		// FIXME no need to allocate this much memory 	
		wchar_t *esc = malloc(32768);
		if (esc == NULL) { printf("malloc() failed in parsing markdown!\n"); }
		md_escape(line, esc, 32769);

		wchar_t *fmt = malloc(32768);
		if (fmt == NULL) { printf("malloc() failed in parsing markdown!\n"); }
		md_inline(esc, fmt);

		if (fmt[0] == L'#') {
			md_header(fmt, output);
		} else if (fmt[0] == L'-' && fmt[1] == L' ') {
			if (!within_list) {
				within_list = 1;
				append(L"<ul>", output, NULL); 
			}
			md_list(fmt, output);
		} else if (fmt[0] == L'>') {
			if (!block_quote) {
				block_quote = 1;
				append(L"<blockquote>", output, NULL);
			}
			md_paragraph(fmt + 1, output);
		} else if ((fmt[0] == '\0') || (fmt[0] == '\n')) {
			md_empty_line(output);
		} else {
			if (within_list) {
				within_list = 0;
				append(L"</ul>", output, NULL);
			} 
			if (block_quote) { 
				block_quote = 0;
				append(L"</blockquote>", output, NULL);
			}
			md_paragraph(fmt, output);
		}
		free(esc);
		free(fmt);
		free(line);

		s = e + 1;
	}		

	// Clean up: did we leave a list open?  Bold?  etc.
	if (within_list) {
		within_list = 0;
		append(L"</ul>", output, NULL);
	} 
	if (bold) 
		append(L"</em>", output, NULL);
	if (italic) 
		append(L"</i>", output, NULL);
	if (block_quote)
		append(L"</blockquote>", output, NULL);
	return output;		  
}

