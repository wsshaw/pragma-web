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
	while (t) {
	//	strip_terminal_newline(t, NULL);
		wcscat(output, L"<a href=\"/t/");
		wcscat(output, t);
		wcscat(output, L".html\">");
		wcscat(output, t);
		wcscat(output, L"</a>");

		t = wcstok(NULL, L",", &tkn);
		wcscat(output, t ? L", " : L"");
	}
	free(in);

	return output;
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

	bool first = true;
	struct tag_dict *tags = malloc(sizeof(tag_dict));
	wchar_t *link_date;

	tags->tag = malloc(64 * sizeof(wchar_t));
	wcscpy(tags->tag, L"beginning");
	tags->next = NULL;

	wchar_t *tag_output = malloc(123456 * sizeof(wchar_t));
	wcscpy(tag_output, site->header);

	wcscat(tag_output, L"<h3>View as: <a href=\"/s/\">scroll</a> | tag index</h3>\n");

	wcscat(tag_output, L"<h2>Tag Index</h2>\n<ul>\n");

        for (pp_page *p = pages; p != NULL ; p = p->next) {
		// FIXME: explode_tags should be changed so that it works here.
		// copy tag list to a new string because wcstok() will modify it.
		wchar_t *tk, *tag_temp = malloc((wcslen(p->tags) + 32) * sizeof(wchar_t));
		wcscpy(tag_temp, p->tags);

		wchar_t *t = wcstok(tag_temp, L",", &tk);

		if (!t) {
			free(tag_temp);
			continue;
		}

		while (t) {
			if (first) {
				// avoid iterating through the list while there's just empty/garbage allocated memory in the first item
				wcscpy(tags->tag, t);
				first = false;
			}
			if (!tag_list_contains(t, tags)) {
				append_tag(t, tags);
			}
			t = wcstok(NULL, L",", &tk);
		}
		free(tag_temp);
	}

	sort_tag_list(tags);
	
	bool in_list = false;

	// Prepare the output canvas for an index of all the pages tagged with a given term
	for (tag_dict *t = tags; t != NULL ; t = t->next) {
		wchar_t *single_tag_index_output = malloc(65536 * sizeof(wchar_t)); 
		wcscpy(single_tag_index_output, site->header);
		wcscat(single_tag_index_output, L"<h2>Pages tagged \"");
		wcscat(single_tag_index_output, t->tag);
		wcscat(single_tag_index_output, L"\"</h2>\n<ul>\n");

		wcscat(tag_output, L"<li><b>");
		wcscat(tag_output, t->tag);
		wcscat(tag_output, L"</b></li>\n");
		for (pp_page *p = pages ; p != NULL ; p = p->next) {
			if (page_is_tagged(p, t->tag)) {	
				if (!in_list) {
					wcscat(tag_output, L"<ul>\n");
					in_list = true;
				}
				// TODO: there is some needless verbosity around generating links, and I don't just mean the
				// hard-coded paths -- need a convenience function in general
				wcscat(single_tag_index_output, L"<li><a href=\"/c/");
				wcscat(tag_output, L"<li><a href=\"/c/");
				
				wchar_t *link_filename = string_from_int(p->date_stamp);
				wcscat(tag_output, link_filename);
				wcscat(single_tag_index_output, link_filename);
				free(link_filename);

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
		strcat(tag_destination, char_convert(t->tag));
		strcat(tag_destination, ".html");

		// Build URL for this individual tag page
		char *base_url_str = char_convert(site->base_url);
		char *tag_str = char_convert(t->tag);
		char *tag_url_str = malloc(256);
		snprintf(tag_url_str, 256, "%st/%s.html", base_url_str, tag_str);
		wchar_t *tag_url = wchar_convert(tag_url_str);
		
		// Apply common token replacements
		single_tag_index_output = apply_common_tokens(single_tag_index_output, site, tag_url, t->tag);
		
		free(base_url_str);
		free(tag_str);
		free(tag_url_str);
		free(tag_url);

		write_file_contents(tag_destination, single_tag_index_output);
		free(tag_destination);
		free(single_tag_index_output);
	}
	wcscat(tag_output, L"</ul>\n");
	wcscat(tag_output, L"<hr>\n");
	wcscat(tag_output, site->footer);
	free(tags);

	// Build URL for main tag index
	wchar_t *tag_index_url = malloc(256 * sizeof(wchar_t));
	wcscpy(tag_index_url, site->base_url);
	wcscat(tag_index_url, L"t/");

	// Apply common token replacements (this handles memory management internally)
	wchar_t *old_output = tag_output;
	tag_output = apply_common_tokens(tag_output, site, tag_index_url, L"All posts");
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
