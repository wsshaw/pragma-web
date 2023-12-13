#include "pragma_poison.h"

/**
* build the site index. Allocates memory that must be freed. 
*/
wchar_t* build_index( pp_page* pages, site_info* site, int start_page ) {
	if (!pages || start_page < 0)
		return NULL;

	int skipahead = start_page * site->index_size;
	int counter = 0;

	// :-D My index page will never exceed 128KB! :-D lol FIXME someday
	wchar_t *index_output = malloc(131072 * sizeof(wchar_t));

	if (!index_output) {
		wprintf(L"! Error allocating memory for building index page (start_page %d)!", start_page);
		perror("malloc(): ");
		return NULL;
	}

	wcscpy(index_output, site->header);

	for (pp_page *current = pages; current != NULL; current = current->next) {
		if (counter <= skipahead && skipahead != 0) { 
			counter++;
			continue;
		} 

		// At this point, we're where we need to be in the list. Render an index with site->index_size items 
		// on it, or as many as we have, whichever is greater.  
		wcscat(index_output, L"<div class=\"post_head\">\n<div class=\"post_icon\">\n");
		wcscat(index_output, L"<img class=\"icon\" alt=\"[AI-generated icon]\" src=\"/img/icons/"); //FIXME 
		wcscat(index_output, current->icon);
		wcscat(index_output, L"\">");
		// TODO: respect the root URL of the site here and elsewhere
		wcscat(index_output, L"</div><div class=\"post_title\"><h3><a href=\"/c/");

		wchar_t *link_filename = string_from_int(current->date_stamp);
		wcscat(index_output, link_filename);
		free(link_filename);

		wcscat(index_output, L".html\">");

		wcscat(index_output, current->title);
		// title
		wcscat(index_output, L"</a></h3><i>Posted on ");

		wchar_t *formatted_date = legible_date(current->date_stamp);
		wcscat(index_output, formatted_date);
		free(formatted_date);

		// date
		wcscat(index_output, L"</i><br>\n");

		// tags
		wchar_t* tag_element = explode_tags(current->tags);
		wcscat(index_output, tag_element);	
		free(tag_element);

		wcscat(index_output, L"</div></div>\n<div>");
		wcscat(index_output, current->content);
		wcscat(index_output, L"</div><hr>\n");
	
		if (current->next == NULL)
			wcscat(index_output, L"<div class=\"foot\">These are the oldest things</div>\n");
	}

	wcscat(index_output, L"</div>\n");	
	wcscat(index_output, site->footer);

        // the header includes some placeholders that need to be removed -- they're mostly for single posts
        index_output = replace_substring(index_output, L"{BACK}", L"");
        index_output = replace_substring(index_output, L"{FORWARD}", L"");
        index_output = replace_substring(index_output, L"{TITLE}", L"");
        index_output = replace_substring(index_output, L"{TAGS}", L"");
        index_output = replace_substring(index_output, L"{DATE}", L"");
	index_output = replace_substring(index_output, L"{PAGETITLE}", site->site_name);

	return index_output;
}

/**
* build a single page.  Allocates meory that must be freed.  
*/
wchar_t* build_single_page(pp_page* page, site_info* site) {
	if (!page || !site)
		return NULL;

	// Try to make reasonable assumptions about the memory to allocate: page, header, footer, some wiggle room
	size_t j = (wcslen(page->content) + wcslen(site->header) + wcslen(site->footer) + 256) * sizeof(wchar_t);
	wchar_t *page_output = malloc(j);

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
	// add the icon. FIXME: relatie paths
	wcscat(page_output, L"<img class=\"icon\" alt=\"[AI-generated icon]\" src=\"/img/icons/");
	wcscat(page_output, page->icon);
	wcscat(page_output, L"\">\n");
	wcscat(page_output, page->content);
	wcscat(page_output, site->footer);

	// Replace the special {magic tokens}! Let's call a special magic function to do so.  
	page_output = replace_substring(page_output, L"{PAGETITLE}", page->title);
	page_output = replace_substring(page_output, L"{FORWARD}", (page->prev ? prev_link : L""));
	page_output = replace_substring(page_output, L"{BACK}", (page->next ? next_link : L"")); 

	// Free memory from string twiddling
	free(previous_id); 
	free(next_id);
	free(prev_link);
	free(next_link);

	wchar_t* title_element = wrap_with_element(page->title, L"<h2>", L"</h2>");
	page_output = replace_substring(page_output, L"{TITLE}", title_element);
	free(title_element);

	wchar_t* tag_element = explode_tags(page->tags);
	page_output = replace_substring(page_output, L"{TAGS}", tag_element);
	free(tag_element);

	wchar_t *formatted_date = wrap_with_element(legible_date(page->date_stamp), L"<i>", L"</i><br>");
	page_output = replace_substring(page_output, L"{DATE}", formatted_date);
	free(formatted_date);

	return page_output;
}

/**
* Given a comma-delimited list of tags, explode them into links to tag index pages.
* Allocates memory that must be freed.  
*/
wchar_t* explode_tags(wchar_t* input) {
	if (!input) 
		return NULL;
	// copy the input somewhere so wcstok() doesn't modify it
	wchar_t* in = malloc((wcslen(input) + 10) * sizeof(wchar_t));

	if (!in) {
		perror("malloc in explode_tags(): ");
		return input;
	}

	wcscpy(in, input);
	strip_terminal_newline(in, NULL);

	wchar_t *tkn;
	wchar_t *t = wcstok(in, L",", &tkn);

	if (!t)
		return in;

	// Realistically speaking, the tag list isn't going to be longer than 1024 characters.  
	// Right?? Nothing can ever go wrong here!
	wchar_t *output = malloc(4096 * sizeof(wchar_t));
	wcscpy(output, L""); 
	while (t) {
	//	strip_terminal_newline(t, NULL);
		wcscat(output, L"<a href=\"/t/");
		wcscat(output, t);
		wcscat(output, L".html\">");
		wcscat(output, t);
		wcscat(output, L"</a>");

		t = wcstok(NULL, L",", &tkn);
		wcscat(output, t ? L", " : L"");
	}
	free(in);

	return output;
}

/**
* build_scroll: generate the site scroll (index of all content).  This output is organized by year (descending) and month/
* day (ascending within year).  
*/
wchar_t* build_scroll(pp_page* pages, site_info* site) {
	if (!pages) 
		return NULL;

	// Figure out the year bounds and how many years there are...
	int min = INT_MAX, max = 0;
	int c = 0, actual_year = 0;
	struct tm *tm_info;

	for (pp_page *p = pages; p != NULL ; p = p->next) {
		tm_info = localtime(&p->date_stamp);
		actual_year = tm_info->tm_year + 1900;

		min = actual_year < min ? actual_year : min;
		max = actual_year > max ? actual_year : max;

		++c;
	}

	// We use a straightforward array structure instead of, say, ***datestamp_list 
	// to keep things readable and simple.  The tradeoff is that it's less efficient
	// in terms of memory usage and has an arbitrary built-in limit of 128 posts/mo.
	// Update MAX_MONTHLY_POSTS as needed.
	time_t calendar[(max - min) + 1][12][MAX_MONTHLY_POSTS];	

	// Initialize the array with consistent values
	for (int i = 0; i < (max - min) + 1 ; i++) {
		for (int j = 0; j < 12 ; j++) {
			for (int k = 0 ; k < MAX_MONTHLY_POSTS ; k++) {
				calendar[i][j][k] = -1;
			}
		}
	}

	for (pp_page *p = pages; p != NULL; p = p->next) {
		tm_info = localtime(&p->date_stamp);
		actual_year = (tm_info->tm_year + 1900) - min;	// "actual" d/b/a "array offset"
		// Find the next available bucket to store this timestamp
		for (int i = 0 ; i < MAX_MONTHLY_POSTS; i++ ) {
			if (calendar[actual_year][tm_info->tm_mon][i] == -1) {
				calendar[actual_year][tm_info->tm_mon][i] = p->date_stamp;
				break;
			}
		}
	}

	// allocate memory for output based on # of pages + header + footer + wiggle room
	wchar_t *scroll_output = malloc(((c * 256) + wcslen(site->footer) + wcslen(site->header)) * sizeof(wchar_t));

    wcscpy(scroll_output, site->header);
	wcscat(scroll_output,L"<h3>View as: scroll | <a href=\"/t/\">tag index</a></h3>\n");

	pp_page *item;
	wchar_t *year, *link_filename, *link_date;
	struct tm t;

	// TODO: Make this a little smarter -- account for empty years (possible)
	for (int i = (max - min) ; i > -1 ; i--) {
		wcscat(scroll_output, L"<h2>");
		year = string_from_int(min + i);
		wcscat(scroll_output, year);
		free(year);
		wcscat(scroll_output, L"</h2>\n<ul>\n");
		for (int j = 11 ; j > -1; j--) {
			for (int k = 0; k < MAX_MONTHLY_POSTS ; k++) {
				if (calendar[i][j][k] == -1) {
					break;
				} 
				
				item = get_item_by_key(calendar[i][j][k], pages);

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

				// build a link with the (uh, hard-coded) relative path 
				wcscat(scroll_output, L"<li><a href=\"../c/");

				// assemble the correct filename                
				link_filename = string_from_int(item->date_stamp);
                		wcscat(scroll_output, link_filename);
                		free(link_filename);

				wcscat(scroll_output, L".html\">");
				wcscat(scroll_output, item->title);
				wcscat(scroll_output, L"</a> - ");

				link_date = legible_date(item->date_stamp);
				wcscat(scroll_output, link_date);
				free(link_date);

				wcscat(scroll_output, L"</li>\n");

				if (calendar[i][j][k + 1] == -1) {
					wcscat(scroll_output, L"</ul>\n");
					break;
				}
			}
		}

		wcscat(scroll_output, L"</ul>\n");
	}

	wcscat(scroll_output, L"<hr>\n");
	wcscat(scroll_output, site->footer);

	// maybe put this in a function since it's more or less a verbatim retread of above
        scroll_output = replace_substring(scroll_output, L"{BACK}", L"");
        scroll_output = replace_substring(scroll_output, L"{FORWARD}", L"");
        scroll_output = replace_substring(scroll_output, L"{TITLE}", L"");
        scroll_output = replace_substring(scroll_output, L"{TAGS}", L"");
        scroll_output = replace_substring(scroll_output, L"{DATE}", L"");
        scroll_output = replace_substring(scroll_output, L"{PAGETITLE}", L"All posts");

	return scroll_output;
}

/**
* Build and return the tag index.  The logic here really needs to be modularized a bit better.  Improvements
* would include decoupling the individual tag page I/O stuff from here -- it feels like a side effect of something
* that announces its intention to simply return a string -- and revisiting the entire structure of tags in the
* site data.  Feels a little clunky, in other words, and smells a little bit.  Very fast though.  
*
* add: boolean arg to trigger/skip individual tag page generation?
*/
wchar_t* build_tag_index(pp_page* pages, site_info* site) {
	if (!pages)
		return NULL;

	bool first = true;
	struct tag_dict *tags = malloc(sizeof(tag_dict));
	wchar_t *link_date;

	tags->tag = malloc(64 * sizeof(wchar_t));
	wcscpy(tags->tag, L"beginning");
	tags->next = NULL;

	wchar_t *tag_output = malloc(123456 * sizeof(wchar_t));
	wcscpy(tag_output, site->header);

	wcscat(tag_output, L"<h3>View as: <a href=\"/s/\">scroll</a> | tag index</h3>\n");

	wcscat(tag_output, L"<h2>Tag Index</h2>\n<ul>\n");

        for (pp_page *p = pages; p != NULL ; p = p->next) {
		// FIXME: explode_tags should be changed so that it works here.
		// copy tag list to a new string because wcstok() will modify it...
		wchar_t *tk, *tag_temp = malloc((wcslen(p->tags) + 32) * sizeof(wchar_t));
		wcscpy(tag_temp, p->tags);

		wchar_t *t = wcstok(tag_temp, L",", &tk);

		if (!t) 	
			continue;

		while (t) {
			if (first) {
				// avoid iterating through the list while there's just empty/garbage allocated memory in the first item
				wcscpy(tags->tag, t);
				first = false;
			}
			if (!tag_list_contains(t, tags)) {
				append_tag(t, tags);
			}
			t = wcstok(NULL, L",", &tk);
		}	
	}

	sort_tag_list(tags);
	
	bool in_list = false;

	for (tag_dict *t = tags; t != NULL ; t = t->next) {
		wchar_t *single_tag_index_output = malloc(65536 * sizeof(wchar_t)); //lowest-effort malloc() ever lol come on

		wcscpy(single_tag_index_output, site->header);
		wcscat(single_tag_index_output, L"<h2>Pages tagged \"");
		wcscat(single_tag_index_output, t->tag);
		wcscat(single_tag_index_output, L"\"</h2>\n<ul>\n");

		wcscat(tag_output, L"<li><b>");
		wcscat(tag_output, t->tag);
		wcscat(tag_output, L"</b></li>\n");
		for (pp_page *p = pages ; p != NULL ; p = p->next) {
			if (page_is_tagged(p, t->tag)) {	
				if (!in_list) {
					wcscat(tag_output, L"<ul>\n");
					in_list = true;
				}
				// really don't like the sloppiness and verbosity around generating links, and I don't just mean the
				// hard-coded paths -- need a convenience function in general TODO
				wcscat(single_tag_index_output, L"<li><a href=\"/c/");
				wcscat(tag_output, L"<li><a href=\"/c/");
				
				wchar_t *link_filename = string_from_int(p->date_stamp);
				wcscat(tag_output, link_filename);
				wcscat(single_tag_index_output, link_filename);
				free(link_filename);

				wcscat(single_tag_index_output, L".html\">");
				wcscat(tag_output, L".html\">");

				wcscat(single_tag_index_output, p->title);
				wcscat(tag_output, p->title);			
			
				wcscat(single_tag_index_output, L"</a> on ");	
				wcscat(tag_output, L"</a> on ");
			
                                link_date = legible_date(p->date_stamp);
                                wcscat(tag_output, link_date);
				wcscat(single_tag_index_output, link_date);
                                free(link_date);
				
				wcscat(single_tag_index_output, L"</li>\n");	
				wcscat(tag_output, L"</li>\n");
			}
		}
		if (in_list) {
			wcscat(tag_output, L"</ul><p></p>\n");
			in_list = false;
		}
		wcscat(single_tag_index_output, L"</ul>\n");
		wcscat(single_tag_index_output, site->footer);

		char *tag_destination = malloc(wcslen(site->base_dir) + 64);
		strcpy(tag_destination, char_convert(site->base_dir));
		strcat(tag_destination, "t/");
		strcat(tag_destination, char_convert(t->tag));
		strcat(tag_destination, ".html");
		write_file_contents(tag_destination, single_tag_index_output);
		free(tag_destination);
		free(single_tag_index_output);
	}
	wcscat(tag_output, L"</ul>\n");
	wcscat(tag_output, L"<hr>\n");
	wcscat(tag_output, site->footer);
	free(tags);

        tag_output = replace_substring(tag_output, L"{BACK}", L"");
        tag_output= replace_substring(tag_output, L"{FORWARD}", L"");
        tag_output= replace_substring(tag_output, L"{TITLE}", L"");
        tag_output= replace_substring(tag_output, L"{TAGS}", L"");
        tag_output= replace_substring(tag_output, L"{DATE}", L"");
        tag_output= replace_substring(tag_output, L"{PAGETITLE}", L"All posts");

	return tag_output;
}

void append_tag(wchar_t *tag, tag_dict *tags) {
	if (!tag || !tags)
		return;

	while (tags->next != NULL) {
      		tags = tags->next;
	}
	
	tags->next = malloc(sizeof(tag_dict));
	tags->next->tag = malloc(64 * sizeof(wchar_t));
	wcscpy(tags->next->tag, tag);
	tags->next->next = NULL;
}

bool tag_list_contains(wchar_t *tag, tag_dict *tags) {
	if (!tags || !tag)
		return false;

	while (tags != NULL) {
		if (!wcscmp(tags->tag, tag))
			return true;
		tags = tags->next;
	}
	return false;
}
