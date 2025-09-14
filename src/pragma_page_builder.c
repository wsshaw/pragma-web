#include "pragma_poison.h"

/**
 * build_single_page(): Assemble a full HTML page for a single post.
 *
 * Composes the page by stitching together the site header/footer, post content,
 * and computed navigation links (older/newer). Also resolves token placeholders
 * such as {TITLE}, {DATE}, {TAGS}, {PAGETITLE}, {FORWARD}, {BACK}, {MAIN_IMAGE},
 * and {PAGE_URL}.
 *
 * Memory:
 *  Returns a heap-allocated wide-character buffer containing the final HTML.
 *  The caller is responsible for free()'ing the returned pointer.
 *
 * arguments:
 *  pp_page  *page  (the post to render; must not be NULL)
 *  site_info*site  (site configuration, header/footer, base_url, etc.; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated HTML buffer on success; NULL on error)
 */
wchar_t* build_single_page(pp_page* page, site_info* site) {
	if (!page || !site) {
		if (PRAGMA_DEBUG) ("=> error: no page/site in build_single_page, page = %s, site = %s", !page?"no":"yes", !site?"no":"yes");
		return NULL;
	}
	// figure out where this page lives in the world
	//also, FIXME: smells that we just use this as is; string_from_int() can return null
	wchar_t *link_filename = string_from_int(page->date_stamp);	
	wchar_t *page_url = malloc(512 * sizeof(wchar_t));

	wcscpy(page_url, site->base_url);
	wcscat(page_url, L"c/");	// FIXME - this is a placeholder because content dir is hard-coded as char.
	wcscat(page_url, link_filename);
	wcscat(page_url, L".html");

	free(link_filename);

	// Try to make reasonable assumptions about the memory to allocate: page, header, footer, some wiggle room.
	size_t j = (wcslen(page->content) + wcslen(site->header) + wcslen(site->footer) + 1024) * sizeof(wchar_t);
	wchar_t *page_output = malloc(j);
	wchar_t *share_image = malloc(128 * sizeof(wchar_t));

	if (page->icon) {
		wcscpy(share_image, L"/img/icons/");
		wcscat(share_image, page->icon);
	} else {
		wcscpy(share_image, site->default_image);
	}

	if (!page_output) {
		wprintf(L"! Unable to allocate memory for assembling page `%ls'!\n", page->title);
		perror("malloc(): ");
		return NULL;
	}

	// get links to previous and next posts, if available
	wchar_t *next_id = page->next ? string_from_int(page->next->date_stamp) : NULL;
	wchar_t *previous_id = page->prev ? string_from_int(page->prev->date_stamp) : NULL;

	// build navigation links using the new component function
	wchar_t *prev_href = NULL;
	wchar_t *next_href = NULL;

	if (previous_id) {
		//wprintf("=> has previous id %ls", previous_id);
		prev_href = malloc((wcslen(previous_id) + 6) * sizeof(wchar_t));
		if (prev_href) {
			swprintf(prev_href, wcslen(previous_id) + 6, L"%ls.html", previous_id);
		}
	}

	if (next_id) {
		//wprintf("=> has next id %ls", next_id);
		next_href = malloc((wcslen(next_id) + 6) * sizeof(wchar_t));
		if (next_href) {
			swprintf(next_href, wcslen(next_id) + 6, L"%ls.html", next_id);
		}
	}

	wchar_t *navigation_links = html_navigation_links(prev_href, next_href);

	free(prev_href);
	free(next_href);

	// Use template system to render the complete page
	page_output = render_page_with_template(page, site);
	if (!page_output) {
		wprintf(L"Error: template rendering failed for page '%ls'\n", page->title);
		return NULL;
	} 

	// Replace the {magic tokens}
	page_output = replace_substring(page_output, L"{PAGETITLE}", page->title);
	page_output = replace_substring(page_output, L"{FORWARD}", navigation_links ? navigation_links : L"");
	page_output = replace_substring(page_output, L"{BACK}", L""); // BACK now handled by FORWARD navigation 
	page_output = replace_substring(page_output, L"{MAIN_IMAGE}", share_image);
	page_output = replace_substring(page_output, L"{SITE_NAME}", site->site_name);
	page_output = replace_substring(page_output, L"#MORE", L"");
	
	// Add description for Open Graph and meta tags
	wchar_t *description = get_page_description(page);
	if (description) {
		page_output = replace_substring(page_output, L"{DESCRIPTION}", description);
		free(description);
	} else {
		page_output = replace_substring(page_output, L"{DESCRIPTION}", L"");
	}

	// Free memory from string manufacturing; return page output
	free(previous_id);
	free(next_id);
	free(navigation_links);

	page_output = replace_substring(page_output, L"{TITLE_FOR_META}", page->title);
	page_output = replace_substring(page_output, L"{PAGE_URL}", page_url);

	wchar_t* title_element = html_heading(3, page->title, NULL);
	page_output = replace_substring(page_output, L"{TITLE}", title_element);
	free(title_element);

	//printf("DEBUG: About to call explode_tags with tags: %ls\n", page->tags ? page->tags : L"[NULL]");

	// Check if {TAGS} token exists in page_output before replacement
	if (wcsstr(page_output, L"{TAGS}")) {
		wchar_t* tag_element = explode_tags(page->tags);
		page_output = replace_substring(page_output, L"{TAGS}", tag_element);
		free(tag_element);
	} else {
	}

	wchar_t *formatted_date = wrap_with_element(legible_date(page->date_stamp), L"<i>", L"</i><br>");
	page_output = replace_substring(page_output, L"{DATE}", formatted_date);
	free(formatted_date);

	free(page_url);
	return page_output;
}
