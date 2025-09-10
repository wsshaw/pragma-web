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
	size_t current_length = wcslen(index_output);

	// Find the right spot in the linked list:
	for (pp_page *current = pages; current != NULL; current = current->next) {
		if (PRAGMA_DEBUG) printf(" -> in page loop in index builder\n");
		if (counter <= skipahead && skipahead != 0) { 
			counter++;
			continue;
		} 

		// At this point, we're where we need to be in the list, thanks to the previous 4 lines. 
		// We want to render an index page with site->index_size items on it, or as many as we have, 
		// whichever is greater.  
		// FIXME: here and throughout, we need to abstract the HTML and not generate it directly like this,
		// esp. with a hard-coded icon path
		if (PRAGMA_DEBUG) printf("memory checkpoint: building post header\n");
		wcscat(index_output, L"<div class=\"post_card\"><div class=\"post_head\">\n<div class=\"post_icon\">\n");
		wcscat(index_output, L"<img class=\"icon\" alt=\"[icon]\" src=\"/img/icons/"); 
		wcscat(index_output, current->icon);
		wcscat(index_output, L"\">");
		if (PRAGMA_DEBUG) printf("passed checkpoint\n");

		// Create a link to the post (As above, TODO: insert the actual root URL of the site here and elsewhere.)
		if (PRAGMA_DEBUG) printf("memory checkpoint: building post link\n");
		wcscat(index_output, L"</div><div class=\"post_title\"><h3><a href=\"/c/");
		
		// Debug the timestamp value
		if (PRAGMA_DEBUG) printf("DEBUG: current->date_stamp = %ld (0x%lx)\n", current->date_stamp, current->date_stamp);
		
		// Validate timestamp is reasonable (between 1970 and 2100)
		if (current->date_stamp < 0 || current->date_stamp > 4102444800L) {
			printf("! Warning: invalid timestamp %ld, using 0\n", current->date_stamp);
			current->date_stamp = 0;
		}
		
		wchar_t *link_filename = string_from_int(current->date_stamp);
		if (!link_filename) {
			printf("! Error: string_from_int returned NULL\n");
			link_filename = wcsdup(L"0");
		}
		
		if (PRAGMA_DEBUG) printf("DEBUG: link_filename = '%ls'\n", link_filename);
		wcscat(index_output, link_filename);
		free(link_filename);
		wcscat(index_output, L".html\">");
		
		// Check for corruption right after wcscat
		if (PRAGMA_DEBUG) {
			size_t check_len = wcslen(index_output);
			printf("DEBUG: After link append, buffer length = %zu\n", check_len);
			for (size_t i = check_len > 50 ? check_len - 50 : 0; i < check_len; i++) {
				if (index_output[i] > 0x10FFFF || index_output[i] < 0) {
					printf("! Corruption detected at pos %zu: U+%08X\n", i, (unsigned int)index_output[i]);
				}
			}
		}
		
		if (PRAGMA_DEBUG) printf("passed checkpoint\n");

		// Add page data, beginning with title/date
		if (PRAGMA_DEBUG) printf("memory checkpoint: building post title\n");
		wcscat(index_output, current->title);
		wcscat(index_output, L"</a></h3><i>Posted on ");
		wchar_t *formatted_date = legible_date(current->date_stamp);
		wcscat(index_output, formatted_date);
		free(formatted_date);
		wcscat(index_output, L"</i><br>\n");
		if (PRAGMA_DEBUG) printf("passed checkpoint\n");

		// Add tags
		if (PRAGMA_DEBUG) printf("memory checkpoint: tag exploder\n");
		wchar_t* tag_element = explode_tags(current->tags);
		wcscat(index_output, tag_element);	
		free(tag_element);
		if (PRAGMA_DEBUG) printf("passed checkpoint\n");

		if (PRAGMA_DEBUG) printf("memory checkpoint: adding content, looking for read-more delimiter\n");
		wcscat(index_output, L"</div></div>\n<div class=\"post_in_index\">");
		// content, or all the content up to the "read more" delimiter (which can appear anywhere)
		if (PRAGMA_DEBUG && pages_processed == 1) {
			printf("DEBUG: Processing 'Pear hat' post (post 2)\n");
			printf("DEBUG: Content length: %zu\n", wcslen(current->content));
			printf("DEBUG: Current buffer length before content: %zu\n", wcslen(index_output));
		}
		
		// Allocate enough space for content + potential read-more HTML (about 100 extra chars should be safe)
		size_t content_len = wcslen(current->content);
		size_t extra_space = 150; // Space for read-more HTML + timestamp + safety margin
		wchar_t *clipped_content = malloc((content_len + extra_space) * sizeof(wchar_t));
		if (!clipped_content) {
			printf("! ERROR: Failed to allocate clipped_content\n");
			continue;
		}
		
		bool did_split = split_before(READ_MORE_DELIMITER, current->content, clipped_content);
		if (PRAGMA_DEBUG && pages_processed == 1) {
			printf("DEBUG: split_before returned %s\n", did_split ? "true" : "false");
			printf("DEBUG: clipped_content length: %zu\n", wcslen(clipped_content));
		}
		
		if (did_split) {
			size_t before_readmore = wcslen(clipped_content);
			wcscat(clipped_content, L"<span style=\"font-weight:bold;\">[ <a href=\"/c/");
			
			// Create a new link_filename since the original was freed
			wchar_t *readmore_link = string_from_int(current->date_stamp);
			wcscat(clipped_content, readmore_link);
			free(readmore_link);
			
			wcscat(clipped_content, L".html\">read more &gt;&gt;</a> ]</span>");
			if (PRAGMA_DEBUG && pages_processed == 1) {
				printf("DEBUG: After read-more append, clipped_content length: %zu (was %zu)\n", 
				       wcslen(clipped_content), before_readmore);
			}
		}
		
		if (PRAGMA_DEBUG && pages_processed == 1) {
			printf("DEBUG: About to append clipped_content to index_output\n");
			printf("DEBUG: clipped_content buffer location: %p\n", (void*)clipped_content);
			
			// Check clipped_content for invalid characters
			printf("DEBUG: Validating clipped_content for invalid characters...\n");
			size_t clipped_len = wcslen(clipped_content);
			for (size_t i = 0; i < clipped_len; i++) {
				if (clipped_content[i] > 0x10FFFF || clipped_content[i] < 0) {
					printf("! Invalid character in clipped_content at pos %zu: U+%08X\n", i, (unsigned int)clipped_content[i]);
					// Show some context
					printf("Context around invalid char: ");
					for (size_t j = (i > 10 ? i - 10 : 0); j < i + 10 && j < clipped_len; j++) {
						if (j == i) printf("[INVALID]");
						else if (clipped_content[j] >= 32 && clipped_content[j] < 127) printf("%lc", clipped_content[j]);
						else printf("?");
					}
					printf("\n");
					break;
				}
			}
			printf("DEBUG: clipped_content validation complete\n");
		}
		
		wcscat(index_output, clipped_content);
		
		if (PRAGMA_DEBUG && pages_processed == 1) {
			printf("DEBUG: After appending content, buffer length: %zu\n", wcslen(index_output));
		}
		
		free(clipped_content);
		wcscat(index_output, L"</div></div>\n");
		if (PRAGMA_DEBUG) printf("passed checkpoint\n");

		// Check for corruption after each post
		if (PRAGMA_DEBUG) {
			size_t post_len = wcslen(index_output);
			printf("DEBUG: After post %d, length = %zu\n", pages_processed + 1, post_len);
			for (size_t i = 2900; i < post_len && i < 3100; i++) {
				if (index_output[i] > 0x10FFFF || index_output[i] < 0) {
					printf("! Corruption after post %d at pos %zu: U+%08X\n", pages_processed + 1, i, (unsigned int)index_output[i]);
					printf("Post title: '%ls'\n", current->title);
					printf("Context: ");
					for (size_t j = (i > 10 ? i - 10 : 0); j < i + 10 && j < post_len; j++) {
						if (j == i) printf("[CORRUPT]");
						else if (index_output[j] >= 32 && index_output[j] < 127) printf("%lc", index_output[j]);
						else printf("?");
					}
					printf("\n");
					break;
				}
			}
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
