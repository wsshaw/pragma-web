/**
* pragma_io: General I/O functions for loading the site sources, writing rendered HTML back to disk, and 
* 
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <wchar.h>
#include <time.h>

#include "pragma_poison.h"

// Given the path to a source file, parse its contents.  Return pointer to the page structure as outlined above.
pp_page* parse_file(const char* filename) {
	wchar_t line[MAX_LINE_LENGTH];
	pp_page* page = malloc(sizeof(pp_page));
	struct stat file_meta;

	if (page == NULL) {
		printf("Can't allocate memory for page while trying to read %s!\n", filename);
		return NULL;
	}

	page->title = malloc(1024);
	page->tags = malloc(1024);
	page->date = malloc(64);
	page->content = malloc(32768);
	size_t content_size = 32768;

	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		printf("Error opening file while trying to read %s\n", filename);
		free(page);
		return NULL;
	}

	if (stat(filename, &file_meta) != 0) {
		printf("Error reading file information for %s\n", filename);
		free(page);	
		return NULL;
	}
	page->last_modified = file_meta.st_mtime;

	if (page->content == NULL) {
		printf("Can't allocate memory for page content while trying to read %s!\n", filename);
		free(page);
		fclose(file);
		return NULL;
	}

	while (fgetws(line, MAX_LINE_LENGTH, file) != NULL && wcscmp(line, L"###\n") != 0) {
		if (wcsstr(line, L"title:") != NULL) 
			wcscpy(page->title, line + wcslen(L"title:")); 
		else if (wcsstr(line, L"tags:") != NULL)
			wcscpy(page->tags, line + wcslen(L"tags:"));
		else if (wcsstr(line, L"date:") != NULL) {
			wcscpy(page->date, line + wcslen(L"date:"));
			page->date_stamp = (time_t)wcstod(page->date, NULL);
		}
	}

	// Read content
	size_t content_index = 0;
	while (fgetws(line, MAX_LINE_LENGTH, file) != NULL) {
		if (wcscmp(line, L"###\n") == 0)
			break;  // End of content
		size_t line_len = wcslen(line);
		if (content_index + line_len >= content_size) {
			content_size *= 2; // Double the size if content exceeds current allocation
			wchar_t* temp = realloc(page->content, content_size * sizeof(wchar_t));
			if (temp == NULL) {
				wprintf(L"! Error: can't reallocate memory to hold contents of %ls!", page->title);
				free(page->title);
				free(page->tags);
				free(page->date);
				free(page->content);
				free(page);
				fclose(file);
				return NULL;
			}
			page->content = temp;
		}
		wcscpy(page->content + content_index, line);
		content_index += line_len;
	}
	fclose(file);
	return page;
}

site_info* load_site_yaml(char* path) {
	if (!path)
		return NULL;

	//char  fix filnemae lol 
	char* yaml = malloc(1024);
	if (!yaml) return NULL;	// FIXME error handling

	strcpy(yaml, path); 
	strcat(yaml, DEFAULT_YAML_FILENAME);
	
	FILE* file = fopen(yaml, "r");

	if (file == NULL) {
		wprintf(L"! Error: can't open yaml configuration file %s!\n", yaml);
		return NULL;
	}

	free(yaml);

		wchar_t line[MAX_LINE_LENGTH];
		site_info* config = malloc(sizeof(site_info));

 	config->site_name = malloc(1024);
		config->css = malloc(256);
	config->js = malloc(256);
		config->header = malloc(4096);
	config->footer = malloc(4096);
	config->tagline = malloc(256);

		while (fgetws(line, MAX_LINE_LENGTH, file) != NULL) {
		// remove newlines first
		size_t len = wcslen(line);
		if (len > 0 && line[len-1] == L'\n')
			line[len-1] = L'\0';

				if (wcsstr(line, L"site_name:") != NULL)
						wcscpy(config->site_name, line + wcslen(L"site_name:"));
				else if (wcsstr(line, L"css:") != NULL)
						wcscpy(config->css, line + wcslen(L"css:"));
				else if (wcsstr(line, L"header:") != NULL) {
			char* header_path = malloc(1024);
			if (!header_path)
				config->header = NULL; 	//FIXME error handling, anyone?  Anywhere?
			strcpy(header_path, path);
			strcat(header_path, char_convert(line+wcslen(L"header:")));
			wcscpy(config->header, read_file_contents(header_path));
			free(header_path);
		}
				else if (wcsstr(line, L"footer:") != NULL) {
			char* footer_path = malloc(1024);
			if (!footer_path)
				config->footer = NULL;
			strcpy(footer_path, path);
			strcat(footer_path, char_convert(line+wcslen(L"footer:")));
			wcscpy(config->footer, read_file_contents(footer_path));
			free(footer_path);
		}
				/*else if (wcsstr(line, L"js:") != NULL)
				else if (wcsstr(line, L"build_tags:") != NULL)
				else if (wcsstr(line, L"build_scroll:") != NULL) */
		else if (wcsstr(line, L"icons_dir:") != NULL) 
			wcscpy(config->icons_dir, line + wcslen(L"icons_dir:"));
				else if (wcsstr(line, L"index_size:") != NULL) {
			config->index_size = (int) wcstol(line + wcslen(L"index_size:"), NULL, 10);	
			if (config->index_size < 1) { // either wcstol failed or config is bonkers 
				wprintf(L"invalid index size in config file! Defaulting to 10.\n");
				config->index_size = 10;
			}
		}
				else if (wcsstr(line, L"tagline:") != NULL)
			wcscpy(config->tagline, line + wcslen(L"tagline:"));
		else
			wprintf(L"bypassing unknown configuration option %ls.\n", line);
	}

	fclose(file);
	return config;
}

// operation is either full load (all data) or meta load (filenames, tags, etc)
// returns a pointer to a linked list of pp_page elements
pp_page* load_site( int operation, char* directory ) {
	if (operation > 2) { }
		DIR *dir;	   
		struct dirent *ent;
	struct pp_page *head = NULL;  
	struct pp_page *tail = NULL;  

		if ((dir = opendir(directory)) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			// The new post generator will create files with a datestamp + .txt.  This should be smarter 
			// about avoiding stuff like vim swap files
						if (strstr(ent->d_name, ".txt") != NULL) {
				char filename[MAX_LINE_LENGTH];
				// feels silly but max filename length in hfs+ is 256 x UTF-16. 
				// FIXME this needs to use wchar_t -- my filenames are all ASCII, but I ain't everyone
				snprintf(filename, 255, "%s%s", SITE_SOURCES, ent->d_name);
					struct pp_page *parsed_data = parse_file(filename);
							if (parsed_data != NULL) {
									//printf("%s, %s", parsed_data->title, parsed_data->content);
					parsed_data->prev = tail;
					parsed_data->next = NULL;
					if (head == NULL)  // linked list is empty
						head = parsed_data;
					else  // append
						tail->next = parsed_data;
					tail = parsed_data;
							} else
							   		printf("parse_file() returned null while trying to read %s!", ent->d_name);
						} else continue;
		}
				closedir(dir);
		} else {
				perror("Can't open the source directory!  Check to see that it's readable.");
				return NULL;
		} 
	return head;
}

void write_single_page(pp_page* page, char *path) {
	if (!page || !path)
		return;

	//strip_terminal_newline(&path);
	wprintf(L"If I were worth a darn, I'd be writing %ls in %s.\n", page->title, path);
	//strcat(destination_file, ".html");

}

void load_site_icons(char *path) { 

}

void directory_to_array(const char *path, char ***filenames, int *count) {
	DIR *dir = opendir(path);
	if (!dir) {
		perror("Error opening directory");
		exit(EXIT_FAILURE);
	}

	struct dirent *entry;

	*count = 0;
	while ((entry = readdir(dir)) != NULL)
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) 
			(*count)++;

	*filenames = malloc((*count) * sizeof(char *));

	rewinddir(dir);

	int i = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
			(*filenames)[i] = malloc(strlen(entry->d_name) + 1);
			strcpy((*filenames)[i], entry->d_name);
			i++;
		}
	}
	closedir(dir);
}

/**

	directory_to_array(path, &filenames, &count);
	for (int i = 0; i < count; i++) {
		printf("Filename %d: %s\n", i + 1, filenames[i]);, etc, etc.
		free(filenames[i]);
	}
	free(filenames);

*/
