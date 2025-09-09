/**
 * pragma web: general I/O functions 
 * 
 * Includes utility functions for loading the site sources, writing rendered 
 * HTML back to disk, and doing miscellaneous tasks like reading filenames into an 
 * array, loading icons, etc.  
 * 
 * By Will Shaw <wsshaw@gmail.com>
*/

#include "pragma_poison.h"

/**  
 * parse_file(): Given the path to a source file, parse its contents.  Return pointer 
 * to the page structure.
 * 
 * arguments:
 * 	const char* filename (path to the pragma source file, containing yaml/md/html)
 * 
 * returns:
 * 	pp_page* structure for the parsed file, allowing it to be rendered to the site.	
*/
pp_page* parse_file(const utf8_path filename) {
	if (PRAGMA_DEBUG)
		printf("parse_file() => %s\n", filename);
	wchar_t line[MAX_LINE_LENGTH];
	pp_page* page = malloc(sizeof(pp_page));
	struct stat file_meta;

	// Memory problem trying to make space for the page: 
	if (page == NULL) {
		printf("Can't allocate memory for page while trying to read %s!\n", filename);
		return NULL;
	}

	// Allocate memory for the components of the internal representation of the page.
	// (The allocations are checked for success below)
	page->title = malloc(1024);
	page->tags = malloc(1024);
	page->date = malloc(256);
	page->content = malloc(32768);
	page->icon = malloc(256);
	page->static_icon = malloc(512);
	page->parsed = true;
	
	// Initialize static_icon to empty string
	wcscpy(page->static_icon, L"");

	// Again, 32K of content is a realistic value, but there are smarter ways to do this
	size_t content_size = 32768;

	FILE* file = utf8_fopen(filename, "r");
	if (file == NULL) {
		printf("Error opening file while trying to read %s\n", filename);
		free_page(page);
		return NULL;
	}

	if (utf8_stat(filename, &file_meta) != 0) {
		printf("Error reading file information for %s\n", filename);
		free_page(page);	
		fclose(file);
		return NULL;
	}
	// CAUTION: Is this portable? Works on mac OS and Linux with APFS/ext4, but it's not
	// tested with other systems. 
	page->last_modified = file_meta.st_mtime;

	if (page->content == NULL) {
		printf("Can't allocate memory for page content while trying to read %s!\n", filename);
		// clean things up if we run into trouble here
		free_page(page);
		fclose(file);
		return NULL;
	}

	// Read the file into wide-character strings until we hit the standard delimiter that 
	// goes between yaml (metadata) and HTML/md (content). FIXME: it's hard-coded at the moment.
	while (fgetws(line, MAX_LINE_LENGTH, file) != NULL && wcscmp(line, L"###\n") != 0) {
		// ...and for each line we read, see if it provides a metadata directive
		if (wcsstr(line, L"title:") != NULL) {
			wcscpy(page->title, line + wcslen(L"title:")); 
			strip_terminal_newline(page->title, NULL);	
		}
		else if (wcsstr(line, L"tags:") != NULL) {
			wcscpy(page->tags, line + wcslen(L"tags:"));
			strip_terminal_newline(page->tags, NULL);	
		}
		else if (wcsstr(line, L"date:") != NULL) {
			wcscpy(page->date, line + wcslen(L"date:"));
			page->date_stamp = (time_t)wcstod(page->date, NULL);
		}
		else if (wcsstr(line, L"static_icon:") != NULL) {
			wcscpy(page->static_icon, line + wcslen(L"static_icon:"));
			strip_terminal_newline(page->static_icon, NULL);
			// Remove leading whitespace
			wchar_t *src = page->static_icon;
			while (*src == L' ' || *src == L'\t') src++;
			if (src != page->static_icon) {
				wmemmove(page->static_icon, src, wcslen(src) + 1);
			}
		}
		else if (wcsstr(line, L"parse:") != NULL) {
    		const wchar_t *value_start = line + wcslen(L"parse:");
			// This is about whether the markdown parser should run on this content, but
			// it needs to be simplified. 
			while (*value_start == L' ' || *value_start == L'\t') value_start++;
			wchar_t temp[256]; 
			wcsncpy(temp, value_start, sizeof(temp)/sizeof(temp[0]) - 1);
			temp[sizeof(temp)/sizeof(temp[0]) - 1] = L'\0';
			strip_terminal_newline(temp, NULL);
    
			if (wcscasecmp(temp, L"true") == 0) {
				page->parsed = true;
    		} else {
				page->parsed = false;
    		}
		}
	}

	// Read post content
	size_t content_index = 0;
	while (fgetws(line, MAX_LINE_LENGTH, file) != NULL) {
		if (wcscmp(line, L"###\n") == 0)
			break;  // End of content
		size_t line_len = wcslen(line);
		if (content_index + line_len >= content_size) {
			// CAUTION - need to audit these casual memory allocations.
			content_size *= 2; // Double the size if content exceeds current allocation
			wchar_t* temp = realloc(page->content, content_size * sizeof(wchar_t));
			if (temp == NULL) {
				// Something's wrong with memory allocation, so get out as cleanly as we can
				wprintf(L"! Error: can't reallocate memory to hold contents of %ls!", page->title);
				free_page(page);
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

/**
 * load_site_yaml(): Read in the configuration data for the whole site from the pragma_config.yml
 * file. FIXME: A key piece of technical debt here is the use of character instead of wide-character
 * strings for file paths. It works but is a known source of code smell.
 * 
 * arguments:
 * 	char* path (use the absolute path to the yaml configuration file)
 * 
 * returns:
 *  site_info* (pointer to a data structure containing the site configuration info)
 */
site_info* load_site_yaml(char* path) {
	if (!path)
		return NULL;

	char *yaml;
	if (asprintf(&yaml, "%s%s", path, DEFAULT_YAML_FILENAME) == -1) return NULL;

	// Assemble filename and open it up
	strcpy(yaml, path); 
	strcat(yaml, DEFAULT_YAML_FILENAME);
	
	FILE* file = utf8_fopen(yaml, "r");
	if (file == NULL) {
		// or bail if we can't open it at all
		wprintf(L"! Error: can't open yaml configuration file %s!\n", yaml);
		return NULL;
	}

	free(yaml);

	// Set up the site config data structure. Allocate memory, read from the file (using wide
	// characters now, as we should be throughout), 
	wchar_t line[MAX_LINE_LENGTH];
	site_info* config = malloc(sizeof(site_info));

 	config->site_name = malloc(1024);
	config->css = malloc(256);
	config->js = malloc(256);
	config->header = malloc(4096 * sizeof(wchar_t));
	config->base_url = malloc(256 * sizeof(wchar_t));
	config->footer = malloc(4096 * sizeof(wchar_t));
	config->tagline = malloc(256);
	config->default_image = malloc(256 * sizeof(wchar_t));
	config->icons_dir = malloc(256 * sizeof(wchar_t));
	config->base_dir = malloc(256 * sizeof(wchar_t));
	config->icons = malloc(123456); // uhhh

	while (fgetws(line, MAX_LINE_LENGTH, file) != NULL) {
		// trim newlines first
		size_t len = wcslen(line);
		if (len > 0 && line[len-1] == L'\n')
			line[len-1] = L'\0';

		if (wcsstr(line, L"site_name:") != NULL)
			wcscpy(config->site_name, line + wcslen(L"site_name:"));
		else if (wcsstr(line, L"css:") != NULL)
			wcscpy(config->css, line + wcslen(L"css:"));
		else if (wcsstr(line, L"base_url:") != NULL)
			wcscpy(config->base_url, line + wcslen(L"base_url:"));
		else if (wcsstr(line, L"default_image:") != NULL)
			wcscpy(config->default_image, line + wcslen(L"default_image:"));
		else if (wcsstr(line, L"header:") != NULL) {
			// TODO: error handling is needed; we also need to check the header file mode;
			// use default header/footer if the proposed header and footer are unusable
			utf8_path header_path = malloc(1024);
			if (!header_path)
				config->header = NULL; 	
			strcpy(header_path, path);
			utf8_path header_suffix = wchar_to_utf8(line+wcslen(L"header:"));
			if (header_suffix) {
				strcat(header_path, header_suffix);
				wcscpy(config->header, read_file_contents(header_path));
				free(header_suffix);
			}
			free(header_path);
		}
		else if (wcsstr(line, L"footer:") != NULL) {
			utf8_path footer_path = malloc(1024);
			if (!footer_path)
				config->footer = NULL;
			strcpy(footer_path, path);
			utf8_path footer_suffix = wchar_to_utf8(line+wcslen(L"footer:"));
			if (footer_suffix) {
				strcat(footer_path, footer_suffix);
				wcscpy(config->footer, read_file_contents(footer_path));
				free(footer_suffix);
			}
			free(footer_path);
		}
		else if (wcsstr(line, L"read_more:") != NULL) {
			config->read_more = (int) wcstol(line + wcslen(L"read_more:"), NULL, 10);
		}
		else if (wcsstr(line, L"icons_dir:") != NULL) 
			wcscpy(config->icons_dir, line + wcslen(L"icons_dir:"));
		else if (wcsstr(line, L"index_size:") != NULL) {
			config->index_size = (int) wcstol(line + wcslen(L"index_size:"), NULL, 10);	
			if (config->index_size < 1) { 
				// either wcstol failed--a condition we might want to check for separately--or 
				// config file is hosed, so find a reasonable default instead of breaking
				wprintf(L"invalid index size in config file! Defaulting to 10.\n");
				config->index_size = 10;
			}
		}
		else if (wcsstr(line, L"tagline:") != NULL)
			wcscpy(config->tagline, line + wcslen(L"tagline:"));
		else // not a known or supported config option
			wprintf(L"bypassing unknown configuration option %ls.\n", line);
	}

	fclose(file);
	return config;
}

/**
 * load_site(): entry function for loading site data from disk. Call directly with the path
 * of a pragma web data directory and a specified operation (full load, refresh metadata, etc.)
 * Support for different loading modes and test runs is forthcoming.
 * 
 * There are a number of things I need to update here: mostly committing to wchar and 
 * handling errors in a consistent and actionable way (use a stable log output format).
 * 
 * 	arguments:
 * 	 int operation (not used: future support for loading modes)
 * 	 char* directory (the *root* path of a pragma site, *not* just the dat/ sources)
 * 
 * returns:
 * 	pp_page* (pointer to a page, i.e., a linked list of pages)
 */
pp_page* load_site( int operation, char* directory ) {
	// Setup: prepare the linked list elements and directory info variables
	DIR *dir;	   
	struct dirent *ent;
	struct pp_page *head = NULL;  
	struct pp_page *tail = NULL;  

	// Assumption is that the directory contains a subdirectory called `dat`, which
	// in turn contains the raw source files. 
	size_t new_length = strlen(directory) + strlen(SITE_SOURCES_DEFAULT_SUBDIR) + 1;
	char* source_directory = malloc(new_length);

	// malloc() has failed.
	if (!source_directory) {
		perror("=> fatal error: can't allocate memory while loading site: ");
		return NULL;
	}

	// Figure out the actual path now that we have all the relevant information...
	strcpy(source_directory, directory);
	strcat(source_directory, SITE_SOURCES_DEFAULT_SUBDIR);

	// ...and try to open it up
	if ((dir = utf8_opendir(source_directory)) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			// The new post generator (helper tool) will create files with a datestamp + .txt.  This code
			// should be smarter about matching that format and avoiding stuff like vim swap files. 
			if (strstr(ent->d_name, ".txt") != NULL) {
			utf8_path filename = malloc(strlen(source_directory) + strlen(ent->d_name) + 1);
			if (!filename) {
				printf("! Error: malloc failed for filename in load_site()\n");
				continue;
			}
			strcpy(filename, source_directory);
			strcat(filename, ent->d_name);
			struct pp_page *parsed_data = parse_file(filename);
			free(filename);
				if (parsed_data != NULL) {
					parsed_data->prev = tail;
					parsed_data->next = NULL;
					if (head == NULL)  // linked list is empty, so give it a head
						head = parsed_data;
					else  // linked list isn't empty, so give it a new tail
						tail->next = parsed_data;
					tail = parsed_data;
				} else
					printf("parse_file() returned null while trying to read %s!", ent->d_name);
			} else continue;
		}
		closedir(dir);
	} else {
		perror("Can't open the source directory! Check to see that it's readable.");
		return NULL;
	} 
	return head;
}
/**
 * write_single_page(): stub/placeholder for function to renderin a single page HTML output.
 **/

void write_single_page(pp_page* page, char *path) {
	if (!page || !path)
		return;
}

/**
 * load_site_icons(): a convenience wrapper that uses the directory_to_array() function
 * (defined below) to represent the post icon filenames in an internal array. It would be
 * more logical for this function to take a unified path as argument 0.
 * 
 * arguments:
 *  char *root (absolute path of the pragma root directory)
 *  char *subdir (subdirectory containing the icons)
 *  site_info *config (data structure containing the config info for this pragma site)
 */
void load_site_icons(char *root, char *subdir, site_info *config) { 
	char *path = malloc(strlen(root) + strlen(subdir) + 1);
	strcpy(path, root);
	strcat(path, subdir);

	// pass an integer pointer to directory_to_array(); it modifies that value based on the
	// final size of the array so we can use it as a sentinel 
	int c;	
   	directory_to_array(path, &config->icons, &c);
	free(path);
	printf("=> Loaded %d icons.\n", c);

	// Save the sentinel value for the icon linked list
	config->icon_sentinel = c;
}

/**
* assign_icons(): give each post a randomly selected icon. Future updates should allow posts to
* specify a stable icon.
*
* arguments:
*  pp_page *pages (linked list of all pages in the site)
*  site_info *config (data structure containing the config info for this pragma site)
*/
void assign_icons(pp_page *pages, site_info *config) {

	// seed the random number generator
	time_t t;
	srand((unsigned)time(&t));
	
	// Need to bail if we don't have the required arguments, of course:
	if (!pages || !config) {
		printf("! error: null %s in assign_icons()\n", pages ? "configuration" : 
			(config ? "page list" : " and page list"));  
		return;
	}

	// assign the icons to each page in the linked list
	for (pp_page *current = pages; current != NULL; current = current->next) {
		bool used_static_icon = false;
		
		// Check if page has a static_icon specified and if file exists
		if (wcslen(current->static_icon) > 0) {
			// Build full path to static icon (relative to site root)
			char *static_icon_path = malloc(512);
			char *base_dir = char_convert(config->base_dir);
			char *static_icon_str = char_convert(current->static_icon);
			
			snprintf(static_icon_path, 512, "%s%s", base_dir, static_icon_str);
			
			// Check if file exists and is readable
			struct stat stat_buf;
			if (stat(static_icon_path, &stat_buf) == 0 && S_ISREG(stat_buf.st_mode)) {
				// File exists and is a regular file - use the static icon path
				wcscpy(current->icon, current->static_icon);
				used_static_icon = true;
			} else {
				printf("! Warning: static_icon '%s' not found or unreadable for post '%ls', using random icon\n", 
					   static_icon_path, current->title);
			}
			
			free(static_icon_path);
			free(base_dir);
			free(static_icon_str);
		}
		
		// If no static icon or file doesn't exist, use random icon
		if (!used_static_icon) {
			wchar_t *the_icon = wchar_convert(config->icons[ rand() % config->icon_sentinel ]);
			wcscpy(current->icon, the_icon);
			free(the_icon);
		}
	}
}

/**
* directory_to_array(): load filenames from a specified directory into an array.  Pass &count to
* keep track of how many files were loaded. 
*
* arguments:
*  const char *path (the directory we want to convert into an array of filenames)
*  char ***filenames (where to store the list of filenames; yes, a pointer to a pointer 
*     to a pointer to a character that gets modified in place. A little ungainly but 
*     a choice for efficiency.)
*  int *count (also a value modifed in place, sentinel calculation)  
*  
*/
void directory_to_array(const utf8_path path, char ***filenames, int *count) {
	// Basics: can we open it?
	DIR *dir = utf8_opendir(path);
	if (!dir) {
		perror("Error opening dir in directory_to_array() ");
		return;
	}

	// iterate over the directory entries, skipping . and .., to arrive at &count.
	// This should probably be a side effect of the loop below.
	struct dirent *entry;
	*count = 0;
	while ((entry = readdir(dir)) != NULL)
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) 
			(*count)++;

	// roll it back and get the acutal names
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
