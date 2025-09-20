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
		wchar_t *scroll_url = build_url(site->base_url, L"s/");
		
		// Apply common token replacements
		wchar_t *temp_scroll = apply_common_tokens(empty_scroll, site, scroll_url, L"#pragma poison | all posts", L"Chronological index of all posts", NULL, NULL, NULL);
		free(empty_scroll);
		empty_scroll = temp_scroll;
		
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

	// Initialize buffer for HTML output
	safe_buffer *output_buf = buffer_pool_get_global();
	if (!output_buf) {
		log_error("Error: failed to get buffer for scroll output\n");
		return NULL;
	}

	// Add header and navigation
	safe_append(site->header, output_buf);

	// Add navigation header with raw HTML to avoid escaping issues
	safe_append(L"<div class=\"post_card\"><h3>View as: scroll | <a href=\"/t/\">tag index</a></h3>\n", output_buf);

	pp_page *item;
	wchar_t *year, *link_date;
	struct tm t;

	// Build the scroll output, organizing it by year and month
	for (int i = (max - min) ; i > -1 ; i--) {
		// Create year heading
		year = string_from_int(min + i);
		wchar_t *year_heading = html_heading(2, year, NULL, true);
		safe_append(year_heading, output_buf);
		safe_append(L"\n", output_buf);
		free(year);
		free(year_heading);

		// Start year list
		safe_append(L"<ul>\n", output_buf);

		for (int j = 11 ; j > -1; j--) {
			for (int k = 0; k < MAX_MONTHLY_POSTS ; k++) {
				if (calendar[i][j][k] == NULL) {
					break;
				}

				item = calendar[i][j][k]; // Direct pointer access - no expensive lookup!

				if (k == 0) {
					// First post of the month - create month heading
					t = *localtime(&item->date_stamp);
					wchar_t month_name[64];
					wcsftime(month_name, 64, L"%B", &t);

					safe_append(L"<li><h3>", output_buf);
					safe_append(month_name, output_buf);
					safe_append(L"</h3></li><ul>\n", output_buf);
				}

				// Build post link and list item with raw HTML
				wchar_t post_url[256];
				swprintf(post_url, 256, L"../c/%ls.html",
					item->source_filename ? item->source_filename : L"unknown");

				link_date = legible_date(item->date_stamp);

				safe_append(L"<li><a href=\"", output_buf);
				safe_append(post_url, output_buf);
				safe_append(L"\">", output_buf);
				safe_append(item->title, output_buf);
				safe_append(L"</a> - ", output_buf);
				safe_append(link_date, output_buf);
				safe_append(L"</li>", output_buf);

				free(link_date);

				// If this is the last post for this month, end the month list
				if (calendar[i][j][k + 1] == NULL) {
					safe_append(L"</ul>\n", output_buf);
					break;
				}
			}
		}

		// End year list
		safe_append(L"</ul>\n", output_buf);
	}

	// Close main div and add footer
	safe_append(L"</div>\n", output_buf);
	safe_append(site->footer, output_buf);

	// Convert buffer to string for token replacement
	wchar_t *scroll_output = wcsdup(output_buf->buffer);
	buffer_pool_return_global(output_buf);

	if (!scroll_output) {
		log_error("Error: failed to convert buffer to string in build_scroll()\n"); 
		return NULL;
	}

	// Build scroll page URL, description 
	wchar_t *scroll_url = build_url(site->base_url, L"s/");
	wchar_t scroll_description[256];
	swprintf(scroll_description, 256, L"Chronological index of all posts on %ls", site->site_name);
	
	// Use "all posts" as page title - the template will combine it with site name
	wchar_t *final_output = apply_common_tokens(scroll_output, site, scroll_url, L"all posts", scroll_description, NULL, NULL, NULL);

	free(scroll_url);
	free(scroll_output);

	return final_output;
}
