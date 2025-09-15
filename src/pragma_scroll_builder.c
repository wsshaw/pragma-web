#include "pragma_poison.h"

/**
 * build_scroll(): Generate the chronological "scroll" index page listing all posts.
 *
 * Computes year bounds from the page list, buckets posts by [year][month][slot]
 * into a fixed-size calendar (MAX_MONTHLY_POSTS per month), and emits grouped
 * HTML with year headings and monthly lists. Prepends site header and appends
 * footer; replaces common {TOKENS}.
 *
 * Memory:
 *  Returns a heap-allocated wide-character buffer containing the full page HTML.
 *  Caller is responsible for free().
 *
 * arguments:
 *  pp_page  *pages (head of the linked list of posts; must not be NULL)
 *  site_info*site  (site configuration: header/footer, defaults, base_url; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated HTML buffer on success; NULL on error)
 *
 * notes:
 *  Uses localtime() per post (not thread-safe); the calendar array is sized from the
 *  observed min/max year range and initialized to -1 for empty slots
 */
wchar_t* build_scroll(pp_page* pages, site_info* site) {
	if (!site) 
		return NULL;

	// Find bounds and count posts in single pass
	int min = INT_MAX, max = 0;
	int c = 0, actual_year = 0;
	struct tm *tm_info;

	// First pass: just find min/max years and count posts
	for (pp_page *p = pages; p != NULL ; p = p->next) {
		tm_info = localtime(&p->date_stamp);
		actual_year = tm_info->tm_year + 1900;

		min = actual_year < min ? actual_year : min;
		max = actual_year > max ? actual_year : max;

		++c;
	}

	// If no pages were found, return an empty scroll page
	if (c == 0) {
		wchar_t *empty_scroll = malloc((wcslen(site->footer) + wcslen(site->header) + 256) * sizeof(wchar_t));
		wcscpy(empty_scroll, site->header);
		wcscat(empty_scroll, L"<div class=\"post_card\"><h3>View as: scroll | <a href=\"/t/\">tag index</a></h3>\n");
		wcscat(empty_scroll, L"<p>No posts found.</p>\n");
		wcscat(empty_scroll, L"</div>\n");
		wcscat(empty_scroll, site->footer);
		
		// Build scroll page URL  
		wchar_t *scroll_url = malloc(256 * sizeof(wchar_t));
		wcscpy(scroll_url, site->base_url);
		wcscat(scroll_url, L"s/");
		
		// Apply common token replacements
		empty_scroll = apply_common_tokens(empty_scroll, site, scroll_url, L"#pragma poison | all posts");
		
		free(scroll_url);
		return empty_scroll;
	}

	// Store direct pointers to posts instead of timestamps to avoid expensive lookups
	// This changes from O(n²) to O(n) complexity for large datasets
	pp_page* calendar[(max - min) + 1][12][MAX_MONTHLY_POSTS];

	// Initialize the array with NULL pointers
	for (int i = 0; i < (max - min) + 1 ; i++) {
		for (int j = 0; j < 12 ; j++) {
			for (int k = 0 ; k < MAX_MONTHLY_POSTS ; k++) {
				calendar[i][j][k] = NULL;
			}
		}
	}

	// Second pass: organize posts into calendar structure
	for (pp_page *p = pages; p != NULL; p = p->next) {
		tm_info = localtime(&p->date_stamp);
		actual_year = (tm_info->tm_year + 1900) - min;	// actual_year = array offset here
		// Find the next available bucket to store this post pointer
		for (int i = 0 ; i < MAX_MONTHLY_POSTS; i++ ) {
			if (calendar[actual_year][tm_info->tm_mon][i] == NULL) {
				calendar[actual_year][tm_info->tm_mon][i] = p;
				break;
			}
		}
	}

	// allocate memory for output based on # of pages + header + footer + wiggle room
	wchar_t *scroll_output = malloc(((c * 512) + wcslen(site->footer) + wcslen(site->header)) * sizeof(wchar_t));

	// prepend the header and navigation element
    wcscpy(scroll_output, site->header);
	wcscat(scroll_output,L"<div class=\"post_card\"><h3>View as: scroll | <a href=\"/t/\">tag index</a></h3>\n");

	pp_page *item;
	wchar_t *year, *link_date;
	struct tm t;

	// build the scroll output, organizing it by year and month.
	// (TODO: Make this just marginally smarter -- account for empty years, for example)
	for (int i = (max - min) ; i > -1 ; i--) {
		wcscat(scroll_output, L"<h2>");

		year = string_from_int(min + i);
		wcscat(scroll_output, year);
		free(year);

		wcscat(scroll_output, L"</h2>\n<ul>\n");
		for (int j = 11 ; j > -1; j--) {
			for (int k = 0; k < MAX_MONTHLY_POSTS ; k++) {
				if (calendar[i][j][k] == NULL) {
					break;
				}

				item = calendar[i][j][k]; // Direct pointer access - no expensive lookup!

				if (k == 0) {
	                		t = *localtime(&item->date_stamp);
 					wchar_t *temp = malloc(64 * sizeof(wchar_t));
					wcsftime(temp, 64, L"%B", &t); 

					// First of the month; start a list
					wcscat(scroll_output, L"<li><h3>");
					wcscat(scroll_output, temp);
					wcscat(scroll_output, L"</h3></li><ul>\n");
					free(temp);
				}

				// build a list item + link with the relative path, which should not be hard-coded. 
				wcscat(scroll_output, L"<li><a href=\"../c/");

				// assemble the correct filename
				if (item->source_filename) {
					wcscat(scroll_output, item->source_filename);
				}

				// add href attribute of the link...
				wcscat(scroll_output, L".html\">");
				wcscat(scroll_output, item->title);
				wcscat(scroll_output, L"</a> - ");

				// ...and print a human-readable date from the epoch
				link_date = legible_date(item->date_stamp);
				wcscat(scroll_output, link_date);
				free(link_date);

				wcscat(scroll_output, L"</li>\n");

				// if this post is the last one for this month, end the list
				if (calendar[i][j][k + 1] == NULL) {
					wcscat(scroll_output, L"</ul>\n");
					break;
				}
			}
		}

		wcscat(scroll_output, L"</ul>\n");
	}

	wcscat(scroll_output, L"</div>\n");
	wcscat(scroll_output, site->footer);

	// Build scroll page URL  
	wchar_t *scroll_url = malloc(256 * sizeof(wchar_t));
	wcscpy(scroll_url, site->base_url);
	wcscat(scroll_url, L"s/");

	// Apply common token replacements
	scroll_output = apply_common_tokens(scroll_output, site, scroll_url, L"#pragma poison | all posts");
	
	free(scroll_url);

	return scroll_output;
}
