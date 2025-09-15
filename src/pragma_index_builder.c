/**
 * pragma web: index builder functions
 * 
 * This file contains the code required to create the site index, i.e., the front page
 * (index 0) and subsequent indices based on the total number of posts and the preferred
 * index length. 
 * 
 * By Will Shaw <wsshaw@gmail.com>
 */

 #include "pragma_poison.h"

/**
* build_index(): build the site index and return it as a string. Allocates memory that 
* must be freed. 
*
* arguments:
* 	pp_page* pages (linked list of all pages in this site)
*	site_info* site (the site information/configuration data)
*   int start_page (which index to generate, given the page size: 0 is the front page)
*
* returns: 
* 	wchar_t* (the HTML of the index page)
*/
wchar_t* build_index( pp_page* pages, site_info* site, int start_page ) {
	if (!site) {
		printf("got null site_info in build_index()! Cannot build without site info -- aborting.");
		// todo: abort gracefully instead of just offering nullity
		return NULL;
	}
	if (!pages || start_page < 0) {
		printf("null pages or start page < 0");
		return NULL;
	}

	int skipahead = (start_page * site->index_size) - 1; // zero index
	int counter = 0;

	// FIXME: Low priority and would break cleanly, but the index page could easily exceed 128KB.
	size_t buffer_size = 131072;
	wchar_t *index_output = malloc(buffer_size * sizeof(wchar_t));
	

	if (!index_output) {
		wprintf(L"! Error allocating memory for building index page (start_page %d)!", start_page);
		perror("malloc(): ");
		return NULL;
	}

	// insert the HTML of the site header first; initialize page counter
	wcscpy(index_output, site->header);
	int pages_processed = 0;
	
	// Track buffer usage to prevent overflow
	// size_t current_length = wcslen(index_output); // Unused for now

	// Find the right spot in the linked list:
	for (pp_page *current = pages; current != NULL; current = current->next) {
		if (counter <= skipahead && skipahead != 0) { 
			counter++;
			continue;
		} 

		// Use template system to render this index item

		// Validate timestamp is reasonable (between 1970 and 2100)
		if (current->date_stamp < 0 || current->date_stamp > 4102444800L) {
			printf("! Warning: invalid timestamp %ld, using 0\n", current->date_stamp);
			current->date_stamp = 0;
		}

		wchar_t *rendered_item = render_index_item_with_template(current, site);
		if (rendered_item) {
			wcscat(index_output, rendered_item);
			free(rendered_item);
		} else {
			printf("! Warning: template rendering failed for post, skipping\n");
		}

		pages_processed++;
		
		if (pages_processed == site->index_size || current->next == NULL) {
			// we've either reached the end of the page list or processed the required # of pages
			wcscat(index_output, L"<div class=\"foot\">\n");
			if (start_page > 0) {
				// there must be newer content if there's already an index...
				wcscat(index_output, L"<a href=\"index");
				wchar_t *bucket = string_from_int(start_page - 1);
				wcscat(index_output, bucket);
				free(bucket);
				wcscat(index_output, L".html\">&lt; newer </a>");
			}
			if (current->next == NULL) // if nothing's left, make a note of it
				wcscat(index_output, L"(these are the oldest things)\n");
			else {
				// basic logic: there must be older content if pages remain in the list
				if (start_page > 0)
					wcscat(index_output, L" | ");
				wcscat(index_output, L"<a href=\"index");
				wchar_t *bucket = string_from_int(start_page + 1);
				wcscat(index_output, bucket);
				free(bucket);
				wcscat(index_output, L".html\">older &gt;");
			}
			// close footer div and break the loop; logically, we must be done at this point
			wcscat(index_output, L"</div>\n");
			break;
		}
	}

	//wcscat(index_output, L"</div>\n");
	
	
	
	// Check if we'll exceed buffer
	size_t current_len = wcslen(index_output);
	size_t footer_len = wcslen(site->footer);
	if (current_len + footer_len >= buffer_size) {
		printf("! BUFFER OVERFLOW WARNING: current %zu + footer %zu >= buffer %zu\n", 
		       current_len, footer_len, buffer_size);
	}
	
	
	// Use safest possible copy method to avoid any corruption
	size_t current_pos = wcslen(index_output);
	
	
	// Use wmemcpy which is safer than wcscpy/wcscat
	wmemcpy(index_output + current_pos, site->footer, footer_len);
	index_output[current_pos + footer_len] = L'\0';  // Manually null terminate
	
	

	// the header includes some placeholders that need to be removed -- they're mostly for single posts
	
	wchar_t *temp;

	temp = template_replace_token(index_output, L"BACK", L"");
	free(index_output);
	index_output = temp;

	temp = template_replace_token(index_output, L"FORWARD", L"");
	free(index_output);
	index_output = temp;

	temp = template_replace_token(index_output, L"TITLE", L"");
	free(index_output);
	index_output = temp;

	temp = template_replace_token(index_output, L"TAGS", L"");
	free(index_output);
	index_output = temp;

	temp = template_replace_token(index_output, L"DATE", L"");
	free(index_output);
	index_output = temp;
	
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

	temp = template_replace_token(index_output, L"MAIN_IMAGE", full_default_image);
	free(index_output);
	index_output = temp;
	free(full_default_image);

	temp = template_replace_token(index_output, L"SITE_NAME", site->site_name);
	free(index_output);
	index_output = temp;

	temp = template_replace_token(index_output, L"TITLE_FOR_META", site->site_name);
	free(index_output);
	index_output = temp;

	temp = template_replace_token(index_output, L"PAGETITLE", site->site_name);
	free(index_output);
	index_output = temp;

	wchar_t *actual_url = malloc(256 * sizeof(wchar_t));
	wcscpy(actual_url, site->base_url);
	wcscat(actual_url, L"index");
	if (start_page > 0) {
		wchar_t *page_num = string_from_int(start_page);
		wcscat(actual_url, page_num);
		free(page_num);
	}
	wcscat(actual_url, L".html");

	temp = template_replace_token(index_output, L"PAGE_URL", actual_url);
	free(index_output);
	index_output = temp;
	
	free(actual_url);

	return index_output;
}
