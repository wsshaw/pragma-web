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
	if (!page || !site)
		return NULL;

	// figure out where this page lives in the world
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

	wchar_t *next_link = next_id ? malloc((wcslen(next_id) + 32) * sizeof(wchar_t)) : NULL;
	wchar_t *prev_link = previous_id ? malloc((wcslen(previous_id) + 32) * sizeof(wchar_t)) : NULL;

	// older page link
	if (next_id != NULL) {
		wcscpy(next_link, L"<a href=\"");
		wcscat(next_link, next_id);
		wcscat(next_link, L".html\">older</a> | ");
	} 

	// newer page link	
	if (previous_id != NULL) {
		wcscpy(prev_link, L" | <a href=\"");
		wcscat(prev_link, previous_id);
		wcscat(prev_link, L".html\">newer</a>");
	}

	wcscpy(page_output, site->header);

	/* add the icon. FIXME: relative paths. */
	wcscat(page_output, L"<div class=\"post_card\"><div class=\"post_head\"><div class=\"post_icon\"><img class=\"icon\" alt=\"[icon]\" src=\"/img/icons/"); 
	wcscat(page_output, page->icon);
	wcscat(page_output, L"\"></div>\n"); 
	wcscat(page_output, L"<div class=\"post_title\">{TITLE}</div>\n");
	wcscat(page_output, L"{DATE}\n{TAGS}</div>\n");
	wcscat(page_output, page->content);
	wcscat(page_output, L"</div>\n");
	wcscat(page_output, site->footer); 

	// Replace the {magic tokens}
	page_output = replace_substring(page_output, L"{PAGETITLE}", page->title);
	page_output = replace_substring(page_output, L"{FORWARD}", (page->prev ? prev_link : L""));
	page_output = replace_substring(page_output, L"{BACK}", (page->next ? next_link : L"")); 
	page_output = replace_substring(page_output, L"{MAIN_IMAGE}", share_image);
	page_output = replace_substring(page_output, L"{SITE_NAME}", site->site_name);
	page_output = replace_substring(page_output, L"#MORE", L"");

	// Free memory from string manufacturing; return page output
	free(previous_id); 
	free(next_id);
	free(prev_link);
	free(next_link);

	page_output = replace_substring(page_output, L"{TITLE_FOR_META}", page->title);
	page_output = replace_substring(page_output, L"{PAGE_URL}", page_url);

	wchar_t* title_element = wrap_with_element(page->title, L"<h3>", L"</h3>");
	page_output = replace_substring(page_output, L"{TITLE}", title_element);
	free(title_element);

	wchar_t* tag_element = explode_tags(page->tags);
	page_output = replace_substring(page_output, L"{TAGS}", tag_element);
	free(tag_element);

	wchar_t *formatted_date = wrap_with_element(legible_date(page->date_stamp), L"<i>", L"</i><br>");
	page_output = replace_substring(page_output, L"{DATE}", formatted_date);
	free(formatted_date);

	free(page_url);
	return page_output;
}
