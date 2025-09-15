#include "pragma_poison.h"

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
	free(config->license);
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

	// Make a copy so we always return new memory (required contract)
	wchar_t *result = wcsdup(output);
	if (!result) return output; // Fallback if duplication fails

	// Use template system for token replacement with proper memory management
	wchar_t *temp;

	temp = template_replace_token(result, L"BACK", L"");
	if (temp) {
		free(result);
		result = temp;
	}

	temp = template_replace_token(result, L"FORWARD", L"");
	if (temp) {
		free(result);
		result = temp;
	}

	temp = template_replace_token(result, L"TITLE", L"");
	if (temp) {
		free(result);
		result = temp;
	}
	// Note: {TAGS} and {DATE} are handled by individual page builders
	// Build full URL for default image
	wchar_t *full_default_image = malloc(512 * sizeof(wchar_t));
	if (wcsstr(site->default_image, L"://")) {
		// Already a full URL
		wcscpy(full_default_image, site->default_image);
	} else {
		// Make it a full URL
		wcscpy(full_default_image, site->base_url);
		// Remove leading slash if present since base_url should include trailing slash
		const wchar_t *default_path = site->default_image;
		if (*default_path == L'/') {
			default_path++;
		}
		wcscat(full_default_image, default_path);
	}
	temp = template_replace_token(result, L"MAIN_IMAGE", full_default_image);
	if (temp) {
		free(result);
		result = temp;
	}
	free(full_default_image);

	temp = template_replace_token(result, L"SITE_NAME", site->site_name);
	if (temp) {
		free(result);
		result = temp;
	}

	if (page_url) {
		temp = template_replace_token(result, L"PAGE_URL", page_url);
		if (temp) {
			free(result);
			result = temp;
		}
	}

	const wchar_t *meta_title = page_title ? page_title : site->site_name;

	temp = template_replace_token(result, L"TITLE_FOR_META", meta_title);
	if (temp) {
		free(result);
		result = temp;
	}

	temp = template_replace_token(result, L"PAGETITLE", meta_title);
	if (temp) {
		free(result);
		result = temp;
	}

	return result;
}