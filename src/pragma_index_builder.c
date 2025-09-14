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
	if (PRAGMA_DEBUG) printf("memory checkpint: allocating index page\n");
	size_t buffer_size = 131072;
	wchar_t *index_output = malloc(buffer_size * sizeof(wchar_t));
	if (PRAGMA_DEBUG) printf("passed checkpoint.\n");
	

	if (!index_output) {
		wprintf(L"! Error allocating memory for building index page (start_page %d)!", start_page);
		perror("malloc(): ");
		return NULL;
	}

	// insert the HTML of the site header first; initialize page counter 
	if (PRAGMA_DEBUG) printf("memory checkpoint: copying site header to output\n");
	wcscpy(index_output, site->header);
	if (PRAGMA_DEBUG) printf("passed checkpoint.\n");
	int pages_processed = 0;
	
	// Track buffer usage to prevent overflow
	// size_t current_length = wcslen(index_output); // Unused for now

	// Find the right spot in the linked list:
	for (pp_page *current = pages; current != NULL; current = current->next) {
		if (PRAGMA_DEBUG) printf(" -> in page loop in index builder\n");
		if (counter <= skipahead && skipahead != 0) { 
			counter++;
			continue;
		} 

		// Use template system to render this index item
		if (PRAGMA_DEBUG) printf("rendering post with template system\n");

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

		if (PRAGMA_DEBUG) printf("template rendering complete\n");

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

	if (PRAGMA_DEBUG) printf("DEBUG: Before final </div>, length = %zu\n", wcslen(index_output));
	//wcscat(index_output, L"</div>\n");
	if (PRAGMA_DEBUG) printf("DEBUG: After final </div>, length = %zu\n", wcslen(index_output));
	
	// Check for corruption after </div>
	if (PRAGMA_DEBUG) {
		size_t check_len = wcslen(index_output);
		for (size_t i = check_len > 20 ? check_len - 20 : 0; i < check_len; i++) {
			if (index_output[i] > 0x10FFFF || index_output[i] < 0) {
				printf("! Corruption after </div> at pos %zu: U+%08X\n", i, (unsigned int)index_output[i]);
			}
		}
	}
	
	if (PRAGMA_DEBUG) {
		printf("DEBUG: About to append footer (length %zu)\n", wcslen(site->footer));
		printf("DEBUG: Current buffer length %zu, buffer size %zu\n", wcslen(index_output), buffer_size);
		
		// Check for corruption BEFORE footer append
		printf("DEBUG: Checking for corruption BEFORE footer append...\n");
		size_t pre_len = wcslen(index_output);
		for (size_t i = 2900; i < pre_len && i < 3100; i++) {
			if (index_output[i] > 0x10FFFF || index_output[i] < 0) {
				printf("! PRE-FOOTER corruption at pos %zu: U+%08X\n", i, (unsigned int)index_output[i]);
				// Show context
				printf("Context: ");
				for (size_t j = (i > 10 ? i - 10 : 0); j < i + 10 && j < pre_len; j++) {
					if (j == i) printf("[CORRUPT]");
					else if (index_output[j] >= 32 && index_output[j] < 127) printf("%lc", index_output[j]);
					else printf("?");
				}
				printf("\n");
			}
		}
	}
	
	// Check if we'll exceed buffer
	size_t current_len = wcslen(index_output);
	size_t footer_len = wcslen(site->footer);
	if (current_len + footer_len >= buffer_size) {
		printf("! BUFFER OVERFLOW WARNING: current %zu + footer %zu >= buffer %zu\n", 
		       current_len, footer_len, buffer_size);
	}
	
	// Validate footer content first
	if (PRAGMA_DEBUG) {
		printf("DEBUG: Validating footer content...\n");
		for (size_t i = 0; i < footer_len; i++) {
			if (site->footer[i] > 0x10FFFF || site->footer[i] < 0) {
				printf("! Invalid character in footer at pos %zu: U+%08X\n", i, (unsigned int)site->footer[i]);
				break;
			}
		}
	}
	
	// Use safest possible copy method to avoid any corruption
	size_t current_pos = wcslen(index_output);
	
	if (PRAGMA_DEBUG) printf("DEBUG: About to copy footer from pos %zu to %zu\n", current_pos, current_pos + footer_len);
	
	// Use wmemcpy which is safer than wcscpy/wcscat
	wmemcpy(index_output + current_pos, site->footer, footer_len);
	index_output[current_pos + footer_len] = L'\0';  // Manually null terminate
	
	if (PRAGMA_DEBUG) printf("DEBUG: After footer append, total length = %zu\n", wcslen(index_output));
	
	// Check for corruption after footer
	if (PRAGMA_DEBUG) {
		size_t check_len = wcslen(index_output);
		for (size_t i = 2900; i < check_len && i < 3100; i++) {
			if (index_output[i] > 0x10FFFF || index_output[i] < 0) {
				printf("! Corruption after footer at pos %zu: U+%08X\n", i, (unsigned int)index_output[i]);
			}
		}
	}

	// the header includes some placeholders that need to be removed -- they're mostly for single posts
	if (PRAGMA_DEBUG) {
		printf("Before token replacements, length: %zu\n", wcslen(index_output));
		
		// Check for invalid characters before token replacement
		for (size_t i = 0; i < wcslen(index_output); i++) {
			if (index_output[i] > 0x10FFFF || index_output[i] < 0) {
				printf("! Invalid character found at pos %zu before token replacement: U+%08X\n", i, (unsigned int)index_output[i]);
				break;
			}
		}
	}
	
	wchar_t *original = index_output;
	index_output = replace_substring(index_output, L"{BACK}", L"");
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{FORWARD}", L"");
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{TITLE}", L"");
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{TAGS}", L"");
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{DATE}", L"");
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{MAIN_IMAGE}", site->default_image);
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{SITE_NAME}", site->site_name);
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{TITLE_FOR_META}", site->site_name);
	if (index_output != original) free(original);
	
	original = index_output;
	index_output = replace_substring(index_output, L"{PAGETITLE}", site->site_name);
	if (index_output != original) free(original);

	wchar_t *actual_url = malloc(256 * sizeof(wchar_t));
	wcscpy(actual_url, site->base_url);
	wcscat(actual_url, L"index");
	if (start_page > 0) {
		wchar_t *page_num = string_from_int(start_page);
		wcscat(actual_url, page_num);
		free(page_num);
	}
	wcscat(actual_url, L".html");

	index_output = replace_substring(index_output, L"{PAGE_URL}", actual_url);
	
	free(actual_url);

	return index_output;
}
