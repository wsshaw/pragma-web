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
		if (PRAGMA_DEBUG) printf("=> error: no page/site in build_single_page, page = %s, site = %s\n", !page?"no":"yes", !site?"no":"yes");
		return NULL;
	}
	// figure out where this page lives in the world
	wchar_t *page_url = malloc(512 * sizeof(wchar_t));
	if (!page_url) {
		printf("Error: could not allocate memory for page URL\n");
		return NULL;
	}

	wcscpy(page_url, site->base_url);
	wcscat(page_url, L"c/");	// FIXME - this is a placeholder because content dir is hard-coded as char.
	if (page->source_filename) {
		wcscat(page_url, page->source_filename);
	}
	wcscat(page_url, L".html");

	// Try to make reasonable assumptions about the memory to allocate: page, header, footer, some wiggle room.
	size_t j = (wcslen(page->content) + wcslen(site->header) + wcslen(site->footer) + 1024) * sizeof(wchar_t);
	wchar_t *page_output = malloc(j);
	wchar_t *share_image = malloc(512 * sizeof(wchar_t));

	if (page->icon) {
		// Build full URL for page-specific icon
		wcscpy(share_image, site->base_url);
		wcscat(share_image, L"img/icons/");
		wcscat(share_image, page->icon);
	} else {
		// Use default image - check if it's already a full URL
		if (wcsstr(site->default_image, L"://")) {
			// Already a full URL
			wcscpy(share_image, site->default_image);
		} else {
			// Make it a full URL
			wcscpy(share_image, site->base_url);
			// Remove leading slash if present since base_url should include trailing slash
			const wchar_t *default_path = site->default_image;
			if (*default_path == L'/') {
				default_path++;
			}
			wcscat(share_image, default_path);
		}
	}

	if (!page_output) {
		wprintf(L"! Unable to allocate memory for assembling page `%ls'!\n", page->title);
		perror("malloc(): ");
		return NULL;
	}

	// get links to previous and next posts, if available
	wchar_t *prev_href = NULL;
	wchar_t *next_href = NULL;

	if (page->prev && page->prev->source_filename) {
		size_t href_len = wcslen(page->prev->source_filename) + 6;
		prev_href = malloc(href_len * sizeof(wchar_t));
		if (prev_href) {
			swprintf(prev_href, href_len, L"%ls.html", page->prev->source_filename);
		}
	}

	if (page->next && page->next->source_filename) {
		size_t href_len = wcslen(page->next->source_filename) + 6;
		next_href = malloc(href_len * sizeof(wchar_t));
		if (next_href) {
			swprintf(next_href, href_len, L"%ls.html", page->next->source_filename);
		}
	}

	wchar_t *navigation_links = html_navigation_links(prev_href, next_href,
	                                                  page->prev ? page->prev->title : NULL,
	                                                  page->next ? page->next->title : NULL);

	free(prev_href);
	free(next_href);

	// Use template system to render the complete page
	page_output = render_page_with_template(page, site);
	if (!page_output) {
		wprintf(L"Error: template rendering failed for page '%ls'\n", page->title);
		return NULL;
	} 

	// Remove #MORE delimiter (not a {TOKEN} so use replace_substring)
	// Note: Common tokens (PAGETITLE, FORWARD, BACK, MAIN_IMAGE, SITE_NAME, etc.)
	// are already handled by apply_common_tokens() in render_page_with_template()
	wchar_t *temp = replace_substring(page_output, L"#MORE", L"");
	if (temp != page_output) {
		free(page_output);
		page_output = temp;
	}

	// Add description for Open Graph and meta tags
	// Note: DESCRIPTION might not be handled by apply_common_tokens
	wchar_t *description = get_page_description(page);
	temp = template_replace_token(page_output, L"DESCRIPTION", description ? description : L"");
	if (temp) {
		free(page_output);
		page_output = temp;
	}
	if (description) {
		free(description);
	}

	// Free memory from string manufacturing; return page output
	free(navigation_links);

	// Note: TITLE_FOR_META and PAGE_URL are already handled by apply_common_tokens()
	// Skip these to avoid double replacement

	// Replace page-specific TITLE token (different from TITLE_FOR_META)
	wchar_t* title_element = html_heading(3, page->title, NULL, true);
	temp = template_replace_token(page_output, L"TITLE", title_element);
	if (temp) {
		free(page_output);
		page_output = temp;
	}
	free(title_element);

	//printf("DEBUG: About to call explode_tags with tags: %ls\n", page->tags ? page->tags : L"[NULL]");

	// Replace page-specific TAGS token
	wchar_t* tag_element = explode_tags(page->tags);
	temp = template_replace_token(page_output, L"TAGS", tag_element ? tag_element : L"");
	if (temp) {
		free(page_output);
		page_output = temp;
	}
	if (tag_element) {
		free(tag_element);
	}

	// Replace page-specific DATE token
	wchar_t *formatted_date = wrap_with_element(legible_date(page->date_stamp), L"<i>", L"</i><br>");
	temp = template_replace_token(page_output, L"DATE", formatted_date);
	if (temp) {
		free(page_output);
		page_output = temp;
	}
	free(formatted_date);

	free(page_url);
	return page_output;
}
