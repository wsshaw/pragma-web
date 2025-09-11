#include "pragma_poison.h"

/**
* Array of important directories for the software. (When you create a new site, we iterate over
* this array and create the directories, so there's no key, like an enum of names, in header files)
*/
const char *pragma_directories[] = {
	"/dat/",
	"/img/",
	"/a/",
	"/c/",
	"/t/",
	"/s/"
};

/**
* Basic configuration, JavaScript, CSS, and HTML files that must be created by the software when
* we generate a new site. As with the above array, there's no need to address these items in a 
* semantically legible way; we just need to make sure the files exist. 
*
* That said, the enum below (pragma_file_info) provides a friendly way to address the elements.
*/
const char *pragma_basic_files[] = {
	"/pragma_config.yml",
	"/pragma_last_run.yml",
	"/p.css",
	"/j.js",
	"/a/index.html",
	"_header.html",
	"_footer.html"
};

/**
* Contents of the files above, as defined in the headers -- placeholder strings or default values.
*/
const wchar_t *pragma_basic_file_skeletons[] = {
	DEFAULT_YAML,
	L"0",		// default last run time: forever ago
	DEFAULT_CSS,
	DEFAULT_JAVASCRIPT,
	DEFAULT_ABOUT_PAGE,
	DEFAULT_HEADER,
	DEFAULT_FOOTER
};

/**
* A convenience enum of legible ways to refer to items in the pragma_basic_files[] array. 
*/
enum pragma_file_info {
	CONFIGURATION,
	LAST_RUN,
	CSS,
	JAVASCRIPT,
	ABOUT,
	HEADER,
	FOOTER
};

/**
 * check_dir(): Verify that a directory exists and has the requested permission bit.
 *
 * Checks that `p` exists, is a directory, and its mode includes the specified `mode`
 *
 * arguments:
 *  const char *p (path to test; must not be NULL)
 *  int         mode (permission bit to test for, like S_IRUSR or S_IWUSR)
 *
 * returns:
 *  bool (true if directory exists and has requested permission; false otherwise) 
 */
bool check_dir( const utf8_path p, int mode ) {
	struct stat dir;
	return (utf8_stat(p, &dir) == 0 && S_ISDIR(dir.st_mode) && dir.st_mode & mode);
}

/**
 * usage(): Print the CLI usage string.
 *
 * Writes PRAGMA_USAGE (defined in headers) to stdout.
 *
 * arguments:
 *  void
 *
 * returns:
 *  void */
void usage() {
	printf("%s", PRAGMA_USAGE);
}

/**
 * build_new_pragma_site(): Initialize a new site in the target directory.
 *
 * Creates required subdirectories, writes baseline config/header/footer/JS/CSS files,
 * and reports progress to stdout. Aborts on the first failure.
 *
 * arguments:
 *  char *t (absolute or working-directory-relative path to the site root; must be writable)
 *
 * returns:
 *  void
*/
void build_new_pragma_site( char *t ) {
	int status;
	char path[256]; 	

	// Ideally, this has happened before calling this function, but let's be sure
	if (!check_dir( t, S_IWUSR )) {
		printf("! Error in build_new_pragma_site(): directory not writable (%s)", t);
		return;
	}

	// Create the necessary subdirectories
	printf("=> Making site subdirectories.\n");

	for (int i = 0 ; i < (int)(sizeof(pragma_directories) / sizeof(pragma_directories[0])) ; ++i) {
		strcpy(path,t);
		strcat(path, pragma_directories[i]);

		status = utf8_mkdir(path, 0700);
		if (status == 0) {
			printf("=> Created directory %s\n", path);
		} else { 
			printf("! Error: couldn't create directory %s!\n", pragma_directories[i]);
			perror("Aborting. The source directories for this website won't be properly configured.\n");
			return;
		}
	}
	
	// If we made it this far, the directories have to exist; create the skeleton files
	for (int i = 0 ; i < (int)(sizeof(pragma_basic_files) / sizeof(pragma_basic_files[0])) ; ++i) {
		strcpy(path,t);
		strcat(path, pragma_basic_files[i]);

		status = write_file_contents(path, pragma_basic_file_skeletons[i]);
		if (status == 0) 
			printf("=> Created %s.\n", pragma_basic_files[i]);
		else {
			printf("! Error: couldn't create file %s!\n", pragma_basic_files[i]);
			perror("failure: ");
			return;
		}
		
	}
	printf("Successfully created new site in %s.\n\n", t);
}

/**
 * write_file_contents(): Write wide-character content to a file path.
 *
 * Opens `path` for writing (truncating if it exists) and writes `content` via fwprintf().
 *
 * arguments:
 *  const char    *path    (filesystem path to write; must not be NULL)
 *  const wchar_t *content (null-terminated wide string to write; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on error)
 */
int write_file_contents(const utf8_path path, const wchar_t *content) {	
	FILE *file = utf8_fopen(path, "w");

	if (file == NULL) {
		perror("! Unable to open file for writing in write_file_contents()!");
		return -1; // If we aren't able to open the file, bail and return an error
	}

	// Convert wide characters to UTF-8 and write as bytes
	if (PRAGMA_DEBUG) printf("Converting content to UTF-8, content length: %zu\n", wcslen(content));
	
	// Find where the invalid character might be
	size_t test_len = wcstombs(NULL, content, 0);
	if (test_len == (size_t)-1) {
		if (PRAGMA_DEBUG) {
			// Try to find the problematic character
			printf("! Invalid wide character found. Scanning content...\n");
			for (size_t i = 0; i < wcslen(content); i++) {
				wchar_t temp[2] = {content[i], L'\0'};
				if (wcstombs(NULL, temp, 0) == (size_t)-1) {
					printf("! Invalid character at position %zu: U+%08X\n", i, (unsigned int)content[i]);
					// Show context around the invalid character
					printf("Context: ");
					for (size_t j = (i > 10 ? i - 10 : 0); j < i + 10 && j < wcslen(content); j++) {
						if (j == i) printf("[INVALID]");
						else if (content[j] >= 32 && content[j] < 127) printf("%lc", content[j]);
						else printf("?");
					}
					printf("\n");
					break;
				}
			}
		}
	}
	
	char *utf8_content = char_convert(content);
	if (!utf8_content) {
		fclose(file);
		printf("! Unable to convert content to UTF-8 in write_file_contents() for path: %s\n", path);
		perror("char_convert failed");
		return -1;
	}
	if (PRAGMA_DEBUG) printf("Converted to UTF-8, length: %zu\n", strlen(utf8_content));

	//wprintf(L"%ls", content);

	int written = fprintf(file, "%s", utf8_content);
	free(utf8_content);
	fclose(file);

	if (written < 0) { // <= -1 bytes written...aka something went wrong
		perror("! Unable to write to file in write_file_contents()!");
		return -1;
	}
   	return 0; // Return 0 for success
}

/**
 * read_file_contents(): Read a file into a newly allocated wide-character buffer.
 *
 * Reads the file as wide characters with fgetwc() and returns a heap-allocated buffer.
 * Caller owns the returned memory and must free().
 *
 * arguments:
 *  const char *path (filesystem path to read; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated buffer containing the file contents; NULL on error)
 */
wchar_t* read_file_contents(const utf8_path path) {
	FILE *file = utf8_fopen(path, "r");

	if (!file) {
		perror("Error opening file");
		return NULL; 
	}

	wint_t ch;
	wchar_t *content = NULL;
	size_t contentSize = 0;
	size_t index = 0;

	while ((ch = fgetwc(file)) != WEOF) {
		if (index == contentSize) {
			// Expand the buffer if needed 
			contentSize += 10; 
			wchar_t *temp = realloc(content, contentSize * sizeof(wchar_t));
			if (!temp) {
				perror("realloc()");
				free(content);
				fclose(file);
				return NULL; 
			}
			content = temp;
		}
		content[index++] = ch;
	}

	if (index < contentSize) {
		content[index] = L'\0';
	} else {
		wchar_t *temp = realloc(content, (contentSize + 1) * sizeof(wchar_t));
		if (!temp) {
			perror("realloc()");
			free(content);
			fclose(file);
			return NULL; 
		}
		content = temp;
		content[index] = L'\0';
	}

	// Close the file
	fclose(file);
	return content; 
}

/**
 * is_valid_site(): Quick check that a candidate site directory is readable and exists. 
 *
 * Currently tests only directory existence + read bit. Future versions should validate structure
 *
 * arguments:
 *  const char *path (directory to test; must not be NULL)
 *
 * returns:
 *  bool (true if readable directory; false otherwise)
 */
bool is_valid_site(const utf8_path path) {
	// Can we even read the directory?
	if (!check_dir(path,S_IRUSR))
		return false;
	return true;
}

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
 * get_item_by_key(): Find a page in a list by its timestamp key.
 *
 * Linear search over the doubly linked list for a matching `date_stamp`.
 *
 * arguments:
 *  time_t   target (epoch seconds to match)
 *  pp_page *list   (head of the page list; may be NULL)
 *
 * returns:
 *  pp_page* (pointer to matching node; NULL if not found)
 */
pp_page* get_item_by_key(time_t target, pp_page* list) { 
	if (!list)
		return NULL;
	for (pp_page *c = list ; c != NULL ; c = c->next) 
		if (c->date_stamp == target) 
			return c;
	return NULL;
}

/**
 * parse_site_markdown(): Convert Markdown to HTML across all pages marked for parsing.
 *
 * For each page with `parsed` true, runs parse_markdown(), resizes the content buffer,
 * and replaces the page's `content` with rendered HTML.
 *
 * arguments:
 *  pp_page *page_list (head of page list; may be NULL)
 *
 * returns:
 *  void
*/
void parse_site_markdown(pp_page* page_list) {
	for (pp_page *current = page_list; current != NULL; current = current->next) {
		// if we have explicitly said not to parse this one as markdown, just use raw html
		if (!current->parsed) 
			continue;

		// Parse the content with the markdown parser...
		wchar_t *markdown_out = parse_markdown(current->content);

		// ...and try to reallocate memory in a more efficient way...
		wchar_t *new_content = realloc(current->content, (wcslen(markdown_out)+1) * sizeof(wchar_t));

		// ...perhaps failing... 
		if (new_content == NULL) {
			printf("! warning: couldn't reallocate memory for page content in parse_site()!\n");
			free(markdown_out);
			continue;
		}

		// ...but, we hope, succeeding. Now replace the content with parsed output:
		current->content = new_content;	
		wcscpy(current->content, markdown_out);
		free(markdown_out); // parse_markdown allocates memory but cannot free it before returning
	}
}

/**
 * merge(): Merge two sorted lists of pp_page by date_stamp (descending).
 *
 * Stable merge used by merge_sort(). Maintains prev/next pointers.
 *
 * arguments:
 *  pp_page *left  (head of left list; may be NULL)
 *  pp_page *right (head of right list; may be NULL)
 *
 * returns:
 *  pp_page* (merged list head)
 */
pp_page* merge(pp_page* left, pp_page* right) {
	return (left == NULL || right == NULL) ? (left == NULL ? right : left) :
		(left->date_stamp >= right->date_stamp) ? 
		(left->next = merge(left->next, right), left->next->prev = left, left->prev = NULL, left) :
		(right->next = merge(left, right->next), right->next->prev = right, right->prev = NULL, right); 
}

/**
 * merge_sort(): Sort a pp_page doubly linked list by date_stamp (descending).
 *
 * Typical fast/slow-pointer technique and recursive merge.
 *
 * arguments:
 *  pp_page *head (head of list; may be NULL)
 *
 * returns:
 *  pp_page* (new head of sorted list)
 */
pp_page* merge_sort(pp_page* head) {
	if (head == NULL || head->next == NULL) return head;

	pp_page* middle = head;
	pp_page* fast = head->next;
	// split 'em and sort 'em
	while (fast != NULL) {
		fast = fast->next;
		if (fast != NULL) {
			middle = middle->next;
			fast = fast->next;
		}
	}
	pp_page* right = middle->next;
	middle->next = NULL;
	return merge(merge_sort(head), merge_sort(right));
}

/**
 * sort_site(): Convenience wrapper to sort the site list in place.
 *
 * Calls merge_sort() and stores the returned head back in *head.
 *
 * arguments:
 *  pp_page **head (address of list head; must not be NULL)
 *
 * returns:
 *  void
 */
void sort_site(pp_page** head) {
	*head = merge_sort(*head);
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
 * legible_date(): Convert an epoch timestamp to a formatted wide-character date string.
 *
 * Uses localtime() and wcsftime() with the format "%Y-%m-%d %H:%M:%S".
 * Caller must free the returned buffer.
 *
 * arguments:
 *  time_t when (epoch time)
 *
 * returns:
 *  wchar_t* (heap-allocated date string)
 */
wchar_t* legible_date(time_t when) {
	struct tm t;
	wchar_t *output = malloc(64 * sizeof(wchar_t));

	t = *localtime(&when);
	wcsftime(output, 64, L"%Y-%m-%d %H:%M:%S", &t);
	return output;
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

/**
 * page_is_tagged(): Determine whether page `p` includes tag `t` in its comma-delimited list.
 *
 * Tokenizes p->tags on commas and compares entries after trimming terminal newlines.
 *
 * arguments:
 *  pp_page *p (page to inspect; must not be NULL)
 *  wchar_t *t (tag to match; must not be NULL)
 *
 * returns:
 *  bool (true if found; false otherwise)
 */
bool page_is_tagged(pp_page *p, wchar_t *t) {
	wchar_t *in = wcsdup(p->tags);
	wchar_t *tkn;
	wchar_t *tn = wcstok(in, L",", &tkn);

	if (!tn) {
		free(in);
		return false;
	}

	while (tn) {
		strip_terminal_newline(tn, NULL);
		if (wcscmp(t, tn) == 0)
			return true;
		tn = wcstok(NULL, L",", &tkn);
	}

	free(in);
	return false;
}

/**
 * swap(): Swap the tag strings stored in two tag_dict nodes.
 *
 * Used by bubble sort implementation for tag lists.
 *
 * arguments:
 *  tag_dict *a (first node; must not be NULL)
 *  tag_dict *b (second node; must not be NULL)
 *
 * returns:
 *  void
 */
void swap(tag_dict *a, tag_dict *b) {
	wchar_t *temp = a->tag;
	a->tag = b->tag;
	b->tag = temp;
}

/**
 * sort_tag_list(): Bubble sort a tag_dict linked list by locale-aware comparison.
 *
 * Uses wcscoll() to order tags ascending; in-place re-linking via swap().
 *
 * arguments:
 *  tag_dict *head (head of list; may be NULL)
 *
 * returns:
 *  void
 */
void sort_tag_list(tag_dict *head) {
	int swapped;
	tag_dict *ptr;
	tag_dict *lptr = NULL;

	if (head == NULL)
		return;

	do {
		swapped = 0;
		ptr = head;

		while (ptr->next != lptr) {
			if (wcscoll(ptr->tag, ptr->next->tag) > 0 ) {
				swap(ptr, ptr->next);
				swapped = 1;
			}
			ptr = ptr->next;
		}
		lptr = ptr;
	} while (swapped);
}

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
 * free_page(): Clean up all allocated memory for a pp_page structure.
 *
 * Frees all dynamically allocated members and the structure itself.
 *
 * arguments:
 *  pp_page *page (page structure to free; may be NULL)
 *
 * returns:
 *  void
 */
void free_page(pp_page *page) {
	if (!page)
		return;
		
	free(page->title);
	free(page->tags);
	free(page->date);
	free(page->content);
	free(page->summary);
	free(page->icon);
	// Temporarily comment out to test if this is causing the double-free
	// free(page->static_icon);
	free(page);
}

/**
 * free_site_info(): Clean up all allocated memory for a site_info structure.
 *
 * Frees all dynamically allocated members, including the icons array, and the structure itself.
 *
 * arguments:
 *  site_info *config (site config structure to free; may be NULL)
 *
 * returns:
 *  void
 */
void free_site_info(site_info *config) {
	if (!config)
		return;
		
	free(config->site_name);
	free(config->css);
	free(config->js);
	free(config->header);
	free(config->base_url);
	free(config->footer);
	free(config->tagline);
	free(config->default_image);
	free(config->icons_dir);
	free(config->base_dir);
	
	if (config->icons) {
		for (int i = 0; i < config->icon_sentinel; i++) {
			free(config->icons[i]);
		}
		free(config->icons);
	}
	
	free(config);
}

/**
 * free_page_list(): Clean up an entire linked list of pp_page structures.
 *
 * Iterates through the linked list and frees all allocated memory for each page.
 *
 * arguments:
 *  pp_page *head (head of page list; may be NULL)
 *
 * returns:
 *  void
 */
void free_page_list(pp_page *head) {
	pp_page *current = head;
	pp_page *next;
	
	while (current != NULL) {
		next = current->next;
		free_page(current);
		current = next;
	}
}

/**
 * apply_common_tokens(): Apply standard token replacements to HTML output.
 *
 * Replaces common template tokens like {BACK}, {FORWARD}, {TITLE}, {TAGS}, etc.
 * with appropriate values or empty strings. This centralizes the token replacement
 * logic that was previously duplicated across multiple builder files.
 *
 * arguments:
 *  wchar_t *output (HTML string to process; must not be NULL)
 *  site_info *site (site configuration; must not be NULL)
 *  const wchar_t *page_url (URL for this page; may be NULL for empty replacement)
 *  const wchar_t *page_title (title for meta tags; may be NULL for site name)
 *
 * returns:
 *  wchar_t* (processed HTML with tokens replaced; caller must free)
 */
wchar_t* apply_common_tokens(wchar_t *output, site_info *site, const wchar_t *page_url, const wchar_t *page_title) {
	if (!output || !site)
		return output;

	// Simple approach - just do the replacements like the original code
	// Accept the memory leaks for now to avoid complex management issues
	output = replace_substring(output, L"{BACK}", L"");
	output = replace_substring(output, L"{FORWARD}", L"");
	output = replace_substring(output, L"{TITLE}", L"");
	output = replace_substring(output, L"{TAGS}", L"");
	output = replace_substring(output, L"{DATE}", L"");
	output = replace_substring(output, L"{MAIN_IMAGE}", site->default_image);
	output = replace_substring(output, L"{SITE_NAME}", site->site_name);
	
	if (page_url) {
		output = replace_substring(output, L"{PAGE_URL}", page_url);
	}
	
	const wchar_t *meta_title = page_title ? page_title : site->site_name;
	output = replace_substring(output, L"{TITLE_FOR_META}", meta_title);
	output = replace_substring(output, L"{PAGETITLE}", meta_title);

	return output;
}

/**
 * UTF-8 safe filesystem wrapper functions
 * 
 * These functions ensure proper UTF-8 handling for filesystem operations
 * by validating that paths are properly encoded UTF-8 strings.
 */

/**
 * utf8_fopen(): UTF-8 safe wrapper for fopen().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded file path; must not be NULL)
 *  const char* mode (file access mode; must not be NULL)
 *
 * returns:
 *  FILE* (file pointer on success; NULL on failure)
 */
FILE* utf8_fopen(const utf8_path path, const char* mode) {
    if (!path || !mode) {
        return NULL;
    }
    
    // On Unix systems with proper locale, standard fopen handles UTF-8
    return fopen(path, mode);
}

/**
 * utf8_opendir(): UTF-8 safe wrapper for opendir().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded directory path; must not be NULL)
 *
 * returns:
 *  DIR* (directory pointer on success; NULL on failure)
 */
DIR* utf8_opendir(const utf8_path path) {
    if (!path) {
        return NULL;
    }
    
    return opendir(path);
}

/**
 * utf8_stat(): UTF-8 safe wrapper for stat().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded file path; must not be NULL)
 *  struct stat* buf (stat buffer; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int utf8_stat(const utf8_path path, struct stat* buf) {
    if (!path || !buf) {
        return -1;
    }
    
    return stat(path, buf);
}

/**
 * utf8_mkdir(): UTF-8 safe wrapper for mkdir().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded directory path; must not be NULL)
 *  mode_t mode (directory permissions)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int utf8_mkdir(const utf8_path path, mode_t mode) {
    if (!path) {
        return -1;
    }
    
    return mkdir(path, mode);
}

/**
 * wchar_to_utf8(): Convert wide string to UTF-8 path.
 *
 * Wrapper around char_convert() with explicit UTF-8 semantics.
 *
 * arguments:
 *  const wchar_t *wide_str (source wide string; must not be NULL)
 *
 * returns:
 *  utf8_path (heap-allocated UTF-8 string; NULL on error)
 */
utf8_path wchar_to_utf8(const wchar_t* wide_str) {
    if (!wide_str) {
        return NULL;
    }
    
    // char_convert uses wcstombs which respects UTF-8 locale
    return char_convert(wide_str);
}

/**
 * utf8_to_wchar(): Convert UTF-8 path to wide string.
 *
 * Wrapper around wchar_convert() with explicit UTF-8 semantics.
 *
 * arguments:
 *  const utf8_path utf8_str (source UTF-8 string; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated wide string; NULL on error)
 */
wchar_t* utf8_to_wchar(const utf8_path utf8_str) {
    if (!utf8_str) {
        return NULL;
    }
    
    // wchar_convert uses mbstowcs which respects UTF-8 locale
    return wchar_convert(utf8_str);
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

/**
 * get_last_run_time(): Read the last successful run timestamp from pragma_last_run.yml
 *
 * Reads the timestamp from the pragma_last_run.yml file in the site directory.
 * Returns 0 if the file doesn't exist or can't be read (meaning never run before).
 *
 * arguments:
 *  const char *site_directory (site root directory; must not be NULL)
 *
 * returns:
 *  time_t (last run timestamp, or 0 if never run/error)
 */
time_t get_last_run_time(const char *site_directory) {
    if (!site_directory) {
        return 0;
    }
    
    char *last_run_path = malloc(strlen(site_directory) + 32);
    if (!last_run_path) {
        return 0;
    }
    
    strcpy(last_run_path, site_directory);
    if (site_directory[strlen(site_directory)-1] != '/') {
        strcat(last_run_path, "/");
    }
    strcat(last_run_path, "pragma_last_run.yml");
    
    FILE *file = utf8_fopen(last_run_path, "r");
    free(last_run_path);
    
    if (!file) {
        return 0; // File doesn't exist, treat as never run
    }
    
    char line[256];
    time_t last_run = 0;
    
    if (fgets(line, sizeof(line), file)) {
        // Parse the timestamp from the file
        last_run = (time_t)strtol(line, NULL, 10);
    }
    
    fclose(file);
    return last_run;
}

/**
 * update_last_run_time(): Write the current timestamp to pragma_last_run.yml
 *
 * Updates the pragma_last_run.yml file with the current timestamp to mark
 * a successful site generation run.
 *
 * arguments:
 *  const char *site_directory (site root directory; must not be NULL)
 *
 * returns:
 *  void
 */
void update_last_run_time(const char *site_directory) {
    if (!site_directory) {
        return;
    }
    
    char *last_run_path = malloc(strlen(site_directory) + 32);
    if (!last_run_path) {
        return;
    }
    
    strcpy(last_run_path, site_directory);
    if (site_directory[strlen(site_directory)-1] != '/') {
        strcat(last_run_path, "/");
    }
    strcat(last_run_path, "pragma_last_run.yml");
    
    FILE *file = utf8_fopen(last_run_path, "w");
    free(last_run_path);
    
    if (!file) {
        return; // Can't write, silently fail
    }
    
    time_t now = time(NULL);
    fprintf(file, "%ld\n", (long)now);
    fclose(file);
}


