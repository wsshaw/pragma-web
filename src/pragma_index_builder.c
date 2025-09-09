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
	wchar_t *index_output = malloc(131072 * sizeof(wchar_t));
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
		wchar_t *link_filename = string_from_int(current->date_stamp);
		wcscat(index_output, link_filename);
		free(link_filename);
		wcscat(index_output, L".html\">");
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
		wchar_t *clipped_content = malloc( ( wcslen(current->content) + 1 )* sizeof(wchar_t));
		bool did_split = split_before(READ_MORE_DELIMITER, current->content, clipped_content);
		if (did_split) {
			wcscat(clipped_content, L"<span style=\"font-weight:bold;\">[ <a href=\"/c/");
			wcscat(clipped_content, link_filename);
			wcscat(clipped_content, L".html\">read more &gt;&gt;</a> ]</span>");
		}
		wcscat(index_output, clipped_content);
		free(clipped_content);
		wcscat(index_output, L"</div></div>\n");
		if (PRAGMA_DEBUG) printf("passed checkpoint\n");

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

	wcscat(index_output, L"</div>\n");	
	wcscat(index_output, site->footer);

	// the header includes some placeholders that need to be removed -- they're mostly for single posts
	index_output = replace_substring(index_output, L"{BACK}", L"");
	index_output = replace_substring(index_output, L"{FORWARD}", L"");
	index_output = replace_substring(index_output, L"{TITLE}", L"");
	index_output = replace_substring(index_output, L"{TAGS}", L"");
	index_output = replace_substring(index_output, L"{DATE}", L"");
	index_output = replace_substring(index_output, L"{MAIN_IMAGE}", site->default_image);
	index_output = replace_substring(index_output, L"{SITE_NAME}", site->site_name);
	index_output = replace_substring(index_output, L"{TITLE_FOR_META}", site->site_name);
	index_output = replace_substring(index_output, L"{PAGETITLE}", site->site_name);

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
