#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <wchar.h>
#include <locale.h>
#include <time.h>

#include "pragma_poison.h"

const char *pragma_directories[] = {
	"/dat/",
	"/img/",
	"/a/",
	"/c/",
	"/t/",
	"/s/"
};

const char *pragma_basic_files[] = {
	"/pragma_config.yml",
	"/pragma_last_run.yml",
	"/p.css",
	"/j.js",
	"/a/index.html",
	"_header.html",
	"_footer.html"
};

const wchar_t *pragma_basic_file_skeletons[] = {
	DEFAULT_YAML,
	L"0",
	DEFAULT_CSS,
	DEFAULT_JAVASCRIPT,
	DEFAULT_ABOUT_PAGE,
	DEFAULT_HEADER,
	DEFAULT_FOOTER
};

enum pragma_file_info {
	CONFIGURATION,
	LAST_RUN,
	CSS,
	JAVASCRIPT,
	ABOUT
};

/**
 * See if a directory exists and is readable or writable.
 * 
 * @param p The path to test.
 * @param mode Either S_IRUSR (user-readable) or S_IWUSR (user-writable)
 */
bool check_dir( const char *p, int mode ) {
	struct stat dir;
	return (stat(p,&dir)==0 && S_ISDIR(dir.st_mode) && dir.st_mode & mode);
}

void usage() {
	printf("%s", PRAGMA_USAGE);
}

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

		status = mkdir(path, 0700);
		if (status == 0) {
			printf("=> Created directory %s\n", path);
			// success: create the config file
		} else { 
			printf("! Error: couldn't create directory %s!\n", pragma_directories[i]);
			perror("Aborting. The source directories for this website won't be properly configured.\n");
			return;
		}
	}
	
	// If we made it this far, the directories exist; create the skeleton files
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

int write_file_contents(const char *path, const wchar_t *content) {	
	FILE *file = fopen(path, "w");

	if (file == NULL) {
		perror("! Unable to open file for writing in write_file_contents()!");
		return -1; // If we aren't able to open the file, bail and return an error
	}

	int written = fwprintf(file, L"%ls", content);
	fclose(file);

	if (written < 0) { // Something went wrong :(
		perror("! Unable to write to file in write_file_contents()!");
		return -1;
	}
   	return 0; // Return 0 for success
}

/**
* read_file_contents: Utility function to read a file and return its contents as a wchar_t pointer.
*/
wchar_t* read_file_contents(const char *path) {
	FILE *file = fopen(path, "r");

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
* Figure out whether a given source directory matches the structure we expect and has a config file.
*/
bool is_valid_site(const char *path) {
	// Can we even read the directory?
	if (!check_dir(path,S_IRUSR))
		return false;
	return true;
}

/**
* Append text (*string) to another piece of text (*result), starting at *j (if supplied).  j is mostly
* in case a caller needs to keep track of where we are in string.
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
* get_item_by_key: given a key (timestamp) and list of page structures, find the one that matches and
* return it.  Return NULL if no match or if the list to search is itself NULL.  
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
* parse_site: iterate over the list of loaded pages and call the necessary functions to parse the Markdown.
* This overwrites the content member of the page struct, which also gets reallocated if necessary.
*/
void parse_site_markdown(pp_page* page_list) {
	for (pp_page *current = page_list; current != NULL; current = current->next) {
		// Parse the content with the markdown parser...
		wchar_t *markdown_out = parse_markdown(current->content);

		// ...and try to reallocate memory in a more efficient way
		wchar_t *new_content = realloc(current->content, (wcslen(markdown_out)+1) * sizeof(wchar_t));

		// allocation failed
		if (new_content == NULL) {
			printf("! warning: couldn't reallocate memory for page content in parse_site()!\n");
			free(markdown_out);
			continue;
		}

		// adjust the content
		current->content = new_content;	
		wcscpy(current->content, markdown_out);
		free(markdown_out); // parse_markdown allocates memory but obviously does not free it before returning
	}
}

/**
* Merge sort utility functions -- these are used for sorting the site data structures.  Default behavior is by date,
* descending. 
*/
pp_page* merge(pp_page* left, pp_page* right) {
	return (left == NULL || right == NULL) ? (left == NULL ? right : left) :
		(left->date_stamp >= right->date_stamp) ? 
		(left->next = merge(left->next, right), left->next->prev = left, left->prev = NULL, left) :
		(right->next = merge(left, right->next), right->next->prev = right, right->prev = NULL, right); // :-D
}

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

// Convenience function to sort the site linked list in place
void sort_site(pp_page** head) {
	*head = merge_sort(*head);
}

/**
* Convert wchar_t back to a basic ASCII character string.  A convenience wrapper for wcstombs(). 
*/
char* char_convert(const wchar_t* w) {
	size_t len = wcstombs(NULL, w, 0);
	char *out_str = malloc(len + 1);  

	if (out_str != NULL) {
		wcstombs(out_str, w, len + 1);
		return out_str;  
	} else {
		printf("! Error: malloc() failed while converting string types in char_convert()!\n");
		return NULL;
	}
}

/**
* Convert char to wchar_t. 
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
* replace_substring: given str, find, and replace as arguments, do just that -- replace 'find' with 
* 'replace' in the supplied string, using wmemcpy()/wcscpy(). 
*/
wchar_t* replace_substring(wchar_t *str, const wchar_t *find, const wchar_t *replace) {
	if (str == NULL || find == NULL || replace == NULL)
		return NULL;

	wchar_t *pos = wcsstr(str, find); // location of substr; bail if none
	if (pos == NULL)
		return str;

	// new string length: it's the existing length minus substring to replace plus replacement length
	size_t new_string_length = wcslen(str) - wcslen(find) + wcslen(replace);
	wchar_t *new_str = (wchar_t *)malloc((new_string_length + 1) * sizeof(wchar_t));

	// build the resulting string
	wmemcpy(new_str, str, pos - str);
	wmemcpy(new_str + (pos-str), replace, wcslen(replace));
	wcscpy(new_str + (pos - str) + wcslen(replace), pos + wcslen(find));
		return new_str;
}

/**
* strip_terminal_newline: given either a wide character string or a regular char string (or both,
* if you really want to), remove the last newline, but only if it's at the very end of the string.  
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
* legible_date: given an epoch timestamp, convert it to a legible date, e.g. "24 December 1980."
* Allocates memory that must be freed.
*/
wchar_t* legible_date(time_t when) {
	struct tm t;
	wchar_t *output = malloc(64 * sizeof(wchar_t));

	t = *localtime(&when);
	wcsftime(output, 64, L"%Y-%m-%d %H:%M:%S", &t);
	return output;
}

wchar_t* string_from_int(long int n) {
	if (!n)
		return NULL;

	int c = 0, x = n;

	while (x != 0) { 
		x = x/10; 
		c++;
	}

	wchar_t* str = malloc((c + 1) * sizeof(wchar_t)); 
	swprintf(str, c + 1, L"%ld", n); 

	return str;
}

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
