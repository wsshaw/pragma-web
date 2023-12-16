/**
* pragma_markdown - markdown parsing functions for the pragma poison generator.
* (c) 2023 Will Shaw <will@pragmapoison.org> 
*
* The markdown parser is more "ad hoc" and "functional" than it is "robust" and "well-designed," but it works.
* It supports the following Markdown elements:
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
*
* HTML can be included alongside the markdown.  
*/

#include "pragma_poison.h"

// State indicators for parsing inline/formatting elements that can span multiple lines
int bold = 0, italic = 0, within_unordered_list = 0, within_ordered_list = 0, block_quote = 0, code = 0, underline = 0, indent_level = 0;

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
		wprintf(L"! insufficient output buffer in md_escape()! will not escape markdown\n");
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

// Parse inline elements such as **, *, etc. -- elements that appear within the chunks
// processed by parse_markdown().
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
		} else if (original[i] == L'`') { // code
			append( code ? L"</code>" : L"<code>", output, &j );
			code = 1 - code;
		} else if (original[i] == L'_') { // underline
			append( underline ? L"</u>" : L"<u>", output, &j);
			underline = 1 - underline;
		} else if (original[i] == L'!') { // images 
			if (i + 1 < length && original[i + 1] == L'[') {
				// Find out bounds of the alt text element
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

				// Find out bounds of the image url element
				start = i;
				while (i + 1 < length && (original[i + 1] != L')' && original[i+1] != L' ')) 
					i++;
				end = i;

				size_t img_url_length = end - start + 1;
				wchar_t image_url[img_url_length + 1];

				wcsncpy(image_url, original + start, img_url_length); // was length - 1
				image_url[img_url_length] = L'\0';

				// tk support for caption lol

				// Construct the <img> tag
				append(L"<img class=\"post\" src=\"", output, &j);
				append(image_url, output, &j);
				append(L"\" alt=\"", output, &j);
				append(alt_text, output, &j);
				append(L"\">", output, &j);
				++i;
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
	if (!input)
		return NULL;

	// make some space for output. 2x input is probably a bit much.
	wchar_t *output = malloc((wcslen(input) * 2) * sizeof(wchar_t)); 
	output[0] = L'\0';

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
	
		// allocate memory for escaping literals
		wchar_t *esc = malloc(32768);
		if (esc == NULL) { printf("malloc() failed in parsing markdown!\n"); }
		md_escape(line, esc, 32769);

		wchar_t *fmt = malloc(32768);
		if (fmt == NULL) { printf("malloc() failed in parsing markdown!\n"); }
		md_inline(esc, fmt);

		if (fmt[0] == L'#') {
			// a line starting with # must be a heading
			md_header(fmt, output);
		} else if (fmt[0] == L'-' && fmt[1] == L' ') {
			// a line beginning with "- " is an unordered list item
			if (!within_unordered_list) {
				within_unordered_list = 1;
				append(L"<ul>\n", output, NULL); 
			}
			md_list(fmt, output);
		} else if (iswdigit(fmt[0]) && fmt[1] == L'.') {
			// a line beginning with "1." is an ordered list item
			if (!within_ordered_list) {
				within_ordered_list = 1;
				append(L"<ol>\n", output, NULL);
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
			if (within_unordered_list) {
				within_unordered_list = 0;
				append(L"</ul>", output, NULL);
			} 
			if (within_ordered_list) {
				within_ordered_list = 0;
				append(L"</ol>\n", output, NULL);
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
	if (within_unordered_list) {
		within_unordered_list = 0;
		append(L"</ul>\n", output, NULL);
	} 
	if (within_ordered_list) {
		within_ordered_list = 0;
		append(L"</ol>\n", output, NULL);
	}
	if (bold) 
		append(L"</em>", output, NULL);
	if (italic) 
		append(L"</i>", output, NULL);
	if (block_quote)
		append(L"</blockquote>", output, NULL);
	if (code)
		append(L"</code>", output, NULL);
	if (underline)
		append(L"</u>", output, NULL);	
	return output;		  
}