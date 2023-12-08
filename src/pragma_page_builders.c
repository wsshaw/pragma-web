#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <malloc/malloc.h>
#include <wchar.h>

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
		wprintf(L"Error allocating memory for building index page (start_page %d)!", start_page);
		perror("malloc(): ");
		return NULL;
	}

	wcscpy(index_output, site->header);

	wprintf(L"%ls", index_output);

	for (pp_page *current = pages; current != NULL; current = current->next) {
		if (counter <= skipahead && skipahead != 0) { 
			counter++;
			continue;
		} 
		// At this point, we're where we need to be in the list Render an index with site->index_size items 
		// on it, or as many as we have, whichever is greater.  
		
		// build the output here, decouple the multi-index logic from the index builder, so 
		// we just call this while iterating over the list, counter % 10 with the right start_page, etc
		wcscat(index_output, L"<div class=\"post_head\">\n<div class=\"post_icon\">\n");
		wcscat(index_output, L"<img src=\"/img/icons/582093fd796544ab81fb9d491eae69a3.jpg\" class=\"icon\" alt=\"[AI-generated icon]\">\n");
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
		wprintf(L"Unable to allocate memory for assembling page `%ls'!\n", page->title);
		perror("malloc(): ");
		return NULL;
	}

	wchar_t *next_url;
	wchar_t *previous_url;

	wcscpy(page_output, site->header);
	wcscat(page_output, page->content);
	wcscat(page_output, site->footer);

	// strip newlines
	strip_terminal_newline(page->title, NULL);

	// Replace the special {magic tokens}! Let's call a special magic function to do so.  
	page_output = replace_substring(page_output, L"{PAGETITLE}", page->title);
	page_output = replace_substring(page_output, L"{BACK}", (page->prev ? page->prev->title : L""));
	page_output = replace_substring(page_output, L"{FORWARD}", (page->next ? page->next->title : L"")); 

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
	wcscpy(output, L"Tagged "); 
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

wchar_t* build_scroll(pp_page* pages) {
	return NULL;
}

wchar_t* build_tag_index(pp_page* pages) {
	return NULL;
}
