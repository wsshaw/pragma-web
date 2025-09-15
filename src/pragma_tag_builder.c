#include "pragma_poison.h"

/*
* explode_tags(): Convert a comma-delimited tag list into a linked string of <a> tags.
*
* Splits the incoming wide-character list on commas and emits a sequence of HTML
* links like: <a href="/t/{tag}.html">{tag}</a>, separated by ", ".
*
* Memory:
*  Returns a heap-allocated wide-character buffer that the caller must free().
*  On internal allocation failure, returns the original (copied) input string
*
* arguments:
*  wchar_t *input (comma-delimited tag list; must not be NULL)
*
* returns:
*  wchar_t* (heap-allocated HTML string on success; may return a copy of input on error)
*/
wchar_t* explode_tags(wchar_t* input) {
	// printf("DEBUG: in explode_tags with input\n"); // Debug if needed
	if (!input)
		return NULL;

	// copy the input somewhere so wcstok() doesn't modify it
	wchar_t* in = malloc((wcslen(input) + 10) * sizeof(wchar_t));

	if (!in) {
		perror("malloc in explode_tags(): ");
		return input;
	}

	wcscpy(in, input);
	strip_terminal_newline(in, NULL);

	wchar_t *tkn;
	wchar_t *t = wcstok(in, L",", &tkn);

	if (!t)
		return in;

	wchar_t *output = malloc(4096 * sizeof(wchar_t));
	wcscpy(output, L"");
	bool first_tag = true;
	while (t) {
		// printf("DEBUG: processing token\n"); // Debug if needed

		// Trim leading and trailing whitespace from the tag
		while (*t && iswspace(*t)) t++; // Skip leading whitespace
		wchar_t *end = t + wcslen(t) - 1;
		while (end > t && iswspace(*end)) *end-- = L'\0'; // Remove trailing whitespace

		if (*t) { // Only process non-empty tags
			if (!first_tag) {
				wcscat(output, L", ");
				// printf("DEBUG: added comma\n"); // Debug if needed
			}
			first_tag = false;

			wcscat(output, L"<a href=\"/t/");
			wcscat(output, t);
			wcscat(output, L".html\">");
			wcscat(output, t);
			wcscat(output, L"</a>");
		}

		t = wcstok(NULL, L",", &tkn);
	}
	free(in);

	return output;
}

/**
 * Simple hash table for wide character strings
 */
#define HASH_TABLE_SIZE 1009  // Prime number for better distribution

typedef struct hash_entry {
    wchar_t *key;
    struct hash_entry *next;  // Chain for collisions
} hash_entry;

typedef struct hash_table {
    hash_entry *buckets[HASH_TABLE_SIZE];
    wchar_t **keys;     // Array of unique keys for iteration
    int key_count;
    int key_capacity;
} hash_table;

/**
 * Simple hash function for wide character strings
 */
static unsigned int hash_wstring(const wchar_t *str) {
    unsigned int hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (unsigned int)*str++;
    }
    return hash % HASH_TABLE_SIZE;
}

/**
 * Create a new hash table
 */
static hash_table* create_hash_table() {
    hash_table *table = malloc(sizeof(hash_table));
    memset(table->buckets, 0, sizeof(table->buckets));

    table->key_capacity = 256;  // Start with room for 256 unique tags
    table->keys = malloc(table->key_capacity * sizeof(wchar_t*));
    table->key_count = 0;

    return table;
}

/**
 * Check if key exists in hash table
 */
static bool hash_contains(hash_table *table, const wchar_t *key) {
    unsigned int bucket = hash_wstring(key);
    hash_entry *entry = table->buckets[bucket];

    while (entry) {
        if (wcscmp(entry->key, key) == 0) {
            return true;
        }
        entry = entry->next;
    }
    return false;
}

/**
 * Add key to hash table (only if it doesn't exist)
 */
static bool hash_add(hash_table *table, const wchar_t *key) {
    if (hash_contains(table, key)) {
        return false;  // Already exists
    }

    unsigned int bucket = hash_wstring(key);

    // Create new entry
    hash_entry *entry = malloc(sizeof(hash_entry));
    entry->key = malloc((wcslen(key) + 1) * sizeof(wchar_t));
    wcscpy(entry->key, key);

    // Add to bucket chain
    entry->next = table->buckets[bucket];
    table->buckets[bucket] = entry;

    // Add to keys array for iteration
    if (table->key_count >= table->key_capacity) {
        table->key_capacity *= 2;
        table->keys = realloc(table->keys, table->key_capacity * sizeof(wchar_t*));
    }
    table->keys[table->key_count++] = entry->key;

    return true;  // Successfully added
}

/**
 * Free hash table and all its contents
 */
static void free_hash_table(hash_table *table) {
    if (!table) return;

    // Free all entries
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_entry *entry = table->buckets[i];
        while (entry) {
            hash_entry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(table->keys);
    free(table);
}

/**
 * Compare function for qsort on wide character strings
 */
static int compare_wchar_strings(const void *a, const void *b) {
    const wchar_t *str1 = *(const wchar_t**)a;
    const wchar_t *str2 = *(const wchar_t**)b;
    return wcscmp(str1, str2);
}

/**
 * Simple structure to hold pre-parsed tag data for a page
 */
typedef struct page_tags {
    pp_page *page;
    wchar_t **tags;  // Array of tag strings
    int tag_count;
} page_tags;

/**
 * parse_page_tags(): Parse tags for a single page into an array
 */
static page_tags* parse_page_tags(pp_page *page) {
    if (!page || !page->tags) return NULL;

    page_tags *parsed = malloc(sizeof(page_tags));
    parsed->page = page;
    parsed->tags = NULL;
    parsed->tag_count = 0;

    // Copy tags string for parsing
    wchar_t *tag_temp = malloc((wcslen(page->tags) + 32) * sizeof(wchar_t));
    wcscpy(tag_temp, page->tags);

    // Count tags first
    wchar_t *tkn;
    wchar_t *t = wcstok(tag_temp, L",", &tkn);
    while (t) {
        // Skip leading whitespace
        while (*t && iswspace(*t)) t++;
        if (*t) parsed->tag_count++;
        t = wcstok(NULL, L",", &tkn);
    }

    if (parsed->tag_count == 0) {
        free(tag_temp);
        free(parsed);
        return NULL;
    }

    // Allocate tag array
    parsed->tags = malloc(parsed->tag_count * sizeof(wchar_t*));

    // Parse tags again and store them
    wcscpy(tag_temp, page->tags);
    t = wcstok(tag_temp, L",", &tkn);
    int i = 0;
    while (t && i < parsed->tag_count) {
        // Trim whitespace
        while (*t && iswspace(*t)) t++;
        wchar_t *end = t + wcslen(t) - 1;
        while (end > t && iswspace(*end)) *end-- = L'\0';

        if (*t) {
            parsed->tags[i] = malloc((wcslen(t) + 1) * sizeof(wchar_t));
            wcscpy(parsed->tags[i], t);
            i++;
        }
        t = wcstok(NULL, L",", &tkn);
    }
    parsed->tag_count = i; // Actual count after trimming

    free(tag_temp);
    return parsed;
}

/**
 * free_page_tags(): Free a page_tags structure
 */
static void free_page_tags(page_tags *parsed) {
    if (!parsed) return;

    for (int i = 0; i < parsed->tag_count; i++) {
        free(parsed->tags[i]);
    }
    free(parsed->tags);
    free(parsed);
}

/**
 * page_has_tag(): Check if a parsed page has a specific tag
 */
static bool page_has_tag(page_tags *parsed, const wchar_t *tag) {
    if (!parsed || !tag) return false;

    for (int i = 0; i < parsed->tag_count; i++) {
        if (wcscmp(parsed->tags[i], tag) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * build_tag_index(): Build the tag index page and per-tag listing pages.
 *
 * Iterates all posts to collect unique tags, sorts them, and renders:
 *  (1) a global Tag Index page listing all tags, and
 *  (2) one page per tag with links to matching posts and dates.
 * Uses site header/footer templates and replaces common {TOKENS}.
 *
 * Memory:
 *  Returns a heap-allocated wide-character buffer containing the Tag Index HTML.
 *  Caller must free(). Per-tag pages are written to disk as a side effect.
 *
 * arguments:
 *  pp_page  *pages (head of linked list of posts; must not be NULL)
 *  site_info*site  (site configuration, including header/footer/base_dir; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated HTML for the Tag Index; NULL on error)
 */
wchar_t* build_tag_index(pp_page* pages, site_info* site) {
	if (!pages)
		return NULL;

	wchar_t *link_date;

	// Pre-parse all page tags once
	page_tags **parsed_pages = NULL;
	int page_count = 0;

	// Count pages and allocate array
	for (pp_page *p = pages; p != NULL; p = p->next) {
		page_count++;
	}

	parsed_pages = malloc(page_count * sizeof(page_tags*));
	int parsed_count = 0;

	// Parse all page tags
	for (pp_page *p = pages; p != NULL; p = p->next) {
		page_tags *parsed = parse_page_tags(p);
		if (parsed) {
			parsed_pages[parsed_count++] = parsed;
		}
	}

	// Create hash table for unique tags
	hash_table *unique_tags = create_hash_table();

	// Build unique tag set using hash table (O(1) lookups)
	for (int i = 0; i < parsed_count; i++) {
		for (int j = 0; j < parsed_pages[i]->tag_count; j++) {
			wchar_t *tag = parsed_pages[i]->tags[j];
			hash_add(unique_tags, tag);  // Only adds if not already present
		}
	}

	printf("=> found %d unique tags, sorting...\n", unique_tags->key_count);

	// Sort tags using qsort (O(n log n))
	qsort(unique_tags->keys, unique_tags->key_count, sizeof(wchar_t*), compare_wchar_strings);

	printf("=> generating tag index pages (0/%d)", unique_tags->key_count);
	fflush(stdout);  // Ensure output appears immediately

	wchar_t *tag_output = malloc(123456 * sizeof(wchar_t));
	wcscpy(tag_output, site->header);

	wcscat(tag_output, L"<h3>View as: <a href=\"/s/\">scroll</a> | tag index</h3>\n");

	wcscat(tag_output, L"<h2>Tag Index</h2>\n<ul>\n");
	
	bool in_list = false;

	// Prepare the output canvas for an index of all the pages tagged with a given term
	for (int tag_idx = 0; tag_idx < unique_tags->key_count; tag_idx++) {
		wchar_t *current_tag = unique_tags->keys[tag_idx];

		// Progress indicator - update every 100 tags or at significant milestones
		if ((tag_idx + 1) % 100 == 0 || tag_idx + 1 == unique_tags->key_count) {
			printf("\r=> generating tag index pages (%d/%d)", tag_idx + 1, unique_tags->key_count);
			fflush(stdout);
		}

		wchar_t *single_tag_index_output = malloc(65536 * sizeof(wchar_t));
		wcscpy(single_tag_index_output, site->header);
		wcscat(single_tag_index_output, L"<h2>Pages tagged \"");
		wcscat(single_tag_index_output, current_tag);
		wcscat(single_tag_index_output, L"\"</h2>\n<ul>\n");

		wcscat(tag_output, L"<li><b>");
		wcscat(tag_output, current_tag);
		wcscat(tag_output, L"</b></li>\n");
		for (int i = 0; i < parsed_count; i++) {
			if (page_has_tag(parsed_pages[i], current_tag)) {
				pp_page *p = parsed_pages[i]->page;	
				if (!in_list) {
					wcscat(tag_output, L"<ul>\n");
					in_list = true;
				}
				// TODO: there is some needless verbosity around generating links, and I don't just mean the
				// hard-coded paths -- need a convenience function in general
				wcscat(single_tag_index_output, L"<li><a href=\"/c/");
				wcscat(tag_output, L"<li><a href=\"/c/");
				
				if (p->source_filename) {
					wcscat(tag_output, p->source_filename);
					wcscat(single_tag_index_output, p->source_filename);
				}

				wcscat(single_tag_index_output, L".html\">");
				wcscat(tag_output, L".html\">");

				wcscat(single_tag_index_output, p->title);
				wcscat(tag_output, p->title);			
			
				wcscat(single_tag_index_output, L"</a> on ");	
				wcscat(tag_output, L"</a> on ");
			
                link_date = legible_date(p->date_stamp);
                wcscat(tag_output, link_date);
				wcscat(single_tag_index_output, link_date);
                free(link_date);
				
				wcscat(single_tag_index_output, L"</li>\n");	
				wcscat(tag_output, L"</li>\n");
			}
		}
		if (in_list) {
			wcscat(tag_output, L"</ul><p></p>\n");
			in_list = false;
		}
		wcscat(single_tag_index_output, L"</ul>\n");
		wcscat(single_tag_index_output, site->footer);

		char *tag_destination = malloc(wcslen(site->base_dir) + 64);
		strcpy(tag_destination, char_convert(site->base_dir));
		strcat(tag_destination, "t/");
		strcat(tag_destination, char_convert(current_tag));
		strcat(tag_destination, ".html");

		// Build URL for this individual tag page
		char *base_url_str = char_convert(site->base_url);
		char *tag_str = char_convert(current_tag);
		char *tag_url_str = malloc(256);
		snprintf(tag_url_str, 256, "%st/%s.html", base_url_str, tag_str);
		wchar_t *tag_url = wchar_convert(tag_url_str);

		// Apply common token replacements
		wchar_t tag_description[256];
		swprintf(tag_description, 256, L"Posts tagged with '%ls' on %ls", current_tag, site->site_name);
		wchar_t *temp_output = apply_common_tokens(single_tag_index_output, site, tag_url, current_tag, tag_description);
		free(single_tag_index_output);
		single_tag_index_output = temp_output;
		
		free(base_url_str);
		free(tag_str);
		free(tag_url_str);
		free(tag_url);

		write_file_contents(tag_destination, single_tag_index_output);
		free(tag_destination);
		free(single_tag_index_output);
	}

	printf("\n=> tag index generation complete\n");

	wcscat(tag_output, L"</ul>\n");
	wcscat(tag_output, L"<hr>\n");
	wcscat(tag_output, site->footer);
	// Cleanup pre-parsed page data
	for (int i = 0; i < parsed_count; i++) {
		free_page_tags(parsed_pages[i]);
	}
	free(parsed_pages);

	// Cleanup hash table
	free_hash_table(unique_tags);

	// Build URL for main tag index
	wchar_t *tag_index_url = malloc(256 * sizeof(wchar_t));
	wcscpy(tag_index_url, site->base_url);
	wcscat(tag_index_url, L"t/");

	// Apply common token replacements (this handles memory management internally)
	wchar_t tag_index_description[256];
	swprintf(tag_index_description, 256, L"Index of tags on %ls", site->site_name);
	wchar_t *old_output = tag_output;
	tag_output = apply_common_tokens(tag_output, site, tag_index_url, L"All posts", tag_index_description);
	free(old_output);  // Free the original since apply_common_tokens returns new memory
	
	free(tag_index_url);

	return tag_output;
}

/**
 * append_tag(): Append a tag string to the end of the tag_dict linked list.
 *
 * Traverses to the list tail and allocates a new node containing `tag`.
 *
 * arguments:
 *  wchar_t  *tag  (tag text to append; must not be NULL)
 *  tag_dict *tags (head of the tag list; must not be NULL)
 *
 * returns:
 *  void
 */
void append_tag(wchar_t *tag, tag_dict *tags) {
	if (!tag || !tags)
		return;

	while (tags->next != NULL) {
      		tags = tags->next;
	}
	
	tags->next = malloc(sizeof(tag_dict));
	tags->next->tag = malloc(64 * sizeof(wchar_t));
	wcscpy(tags->next->tag, tag);
	tags->next->next = NULL;
}

/**
 * tag_list_contains(): Determine whether a tag exists in the tag_dict linked list.
 *
 * Performs a case-sensitive comparison of each node's `tag` with the provided value.
 *
 * arguments:
 *  wchar_t  *tag  (tag text to search for; must not be NULL)
 *  tag_dict *tags (head of the tag list; may be NULL)
 *
 * returns:
 *  bool (true if found; false otherwise)
 */
bool tag_list_contains(wchar_t *tag, tag_dict *tags) {
	if (!tags || !tag)
		return false;

	while (tags != NULL) {
		if (!wcscmp(tags->tag, tag))
			return true;
		tags = tags->next;
	}
	return false;
}
