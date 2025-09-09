#include "pragma_poison.h"

/**
 * main(): Entry point for the `pragma web` static site generator.
 *
 * Parses CLI arguments, validates source/output directories, and dispatches
 * site-building operations. 
 *  - Create a new site with -c
 *  - Load site configuration and sources with -s
 *  - Generate output into a specified directory with -o
 *  - Building indices, scroll, tag index, and individual pages
 *
 * arguments:
 *  int   argc (number of command-line arguments)
 *  char *argv[] (array of argument strings)
 *
 * returns:
 *  int (EXIT_SUCCESS or EXIT_FAILURE)
 */
int main(int argc, char *argv[]) {
	// setlocale() needs to be called asap so that Unicode is handled properly; change as needed
	setlocale(LC_CTYPE, "en_US.UTF-8");

	// some significant locations and options we need to keep track of at startup
	char *pragma_source_directory, *pragma_output_directory, *posts_output_directory;
	bool source_specified = false;
	bool destination_specified = false;
	bool dry_run_specified = false;
	bool force_all_specified = false;
	bool updated_only = false;
	bool create_site_specified = false;

	// need at least some kind of argument because there's no default action
	if ( argc == 1 ) {
		usage();
		exit(EXIT_FAILURE); 
	}

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-s") == 0) 
			source_specified = true;
		else if (strcmp(argv[i], "-o") == 0)
			destination_specified = true;
		else if (strcmp(argv[i], "-c") == 0)
			create_site_specified = true;
	}

	// Handle the arguments that govern our behavior.  The flag logic is:
	// 
	// We must have either (-s && -o) || (-c)
	// if we have (-s && -o)
	//	source and output confirmed; check mode flag and act accordingly
	// if we have (-s || -o)
	//	warn user that this isn't going to work because we need both, exit
	// else if we have (-c)
	//	create new site, exit
	//
	// There are libraries that can handle all of this stuff; a possible TODO would be to find one
	// and implement it here -- I'm doing a lot of logical grunt work that could be streamlined.
	// (The technical debt to pay down is shaped like getopts.)

	if (!source_specified) {
		printf("=> Error: must specify site directory with -s\n");
		exit(EXIT_FAILURE);
	} else if (!source_specified && !create_site_specified) {
		printf("=> Error: must either specify source directory (-s) or create new site (-c)\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < argc; i++) {
		// There's no default value for source/output, and output isn't specified in the config
		// file to allow for various staging possibilities/temporary setups.  So they need to be
		// specified here or in whatever script calls pragma builder.
		if (strcmp(argv[i], "-s") == 0) {
			if (i + 1 < argc) {
				// Does the source directory exist?  Is it readable?
				if (!check_dir(argv[i+1], S_IRUSR)) {
					printf("=> Error: source directory (%s) does not exist or is not readable.\n", argv[i+1]);
					printf("=> To create a new site, use pragma -c /dest, where /dest is the directory in which\n");
					printf("=> you'd like to keep the site sources.  Then edit /dest/pragma_config.yml and try again.\n");
					exit(EXIT_FAILURE);
				} else {
					pragma_source_directory = argv[i + 1];
					printf("=> Using source directory %s\n", pragma_source_directory); 
					source_specified = true;
					++i;
				}
			} else {
				printf("! Error: no directory specified after -s.\n");
				exit(EXIT_FAILURE);
			}
		} else if (strcmp(argv[i], "-o") == 0) {
			if (i + 1 < argc) {
				// Does the output directory exist?  Is it writable?
				if (!check_dir(argv[i+1], S_IWUSR)) {
					printf("! Error: output directory (%s) does not exist or is not writable.\n", argv[i+1]);
					exit(EXIT_FAILURE);
				} else {
					pragma_output_directory = argv[i + 1];
					printf("=> Using output directory %s\n", pragma_output_directory);
					destination_specified = true;
					posts_output_directory = malloc(strlen(pragma_output_directory) + strlen(SITE_POSTS) + 64); //save space for filename; FIXME
					if (posts_output_directory == NULL ) {
						printf("! Error: can't allocate memory for string representing the output path...\n");
						exit(EXIT_FAILURE);
					}
					
					// Figure out the posts directory, given the site base output dir....
					strcpy(posts_output_directory, pragma_output_directory);
					// ...and add trailing / if it's not there.
					strcat(posts_output_directory, pragma_output_directory[strlen(pragma_output_directory)-1] != '/' ? "/" : "");
					strcat(posts_output_directory, SITE_POSTS);	
					++i;
				}
			} else {
				printf("Error: no output directory specified after -o.\n");
				exit(EXIT_FAILURE);
			}
		} else if (strcmp(argv[i], "-c") == 0) {
			if (i > 1) {
				if (PRAGMA_DEBUG)
					printf("=> debug => (not in first)");
			}
			if (i + 1 < argc) {
				// Create a new site. We won't try to make a directory that doesn't exist, though.
				if (!check_dir(argv[i+1], S_IWUSR)) {
					printf("=> Error: the specified directory (%s) does not exist or is not writable.\n", argv[i+1]);
					printf("=> Please create the directory and/or adjust permissions to at least 600.\n");
					exit(EXIT_FAILURE);
				} else {
					// Ok, we found a directory we can write to.  Set up the basics of the site.
					build_new_pragma_site(argv[i+1]);
					exit(EXIT_SUCCESS);
				}
			} else {
				printf("Usage: %s -c [destination directory]\n", argv[0]);
				exit(EXIT_SUCCESS);
			}
		// TODO: Implement the logic for these operations (force rebuild, updated files only, dry run)
		} else if (strcmp(argv[i], "-f") == 0) {
		} else if (strcmp(argv[i], "-u") == 0) {
		} else if (strcmp(argv[i], "-n") == 0) {
		} else if (strcmp(argv[i], "-h") == 0) {
			usage();
			exit(EXIT_SUCCESS);
		} else {
			// Skip unknown arguments (including argv[0])
			continue;
		}

	}

	// We learned about a source but don't know where to put the output, so fall back on default.
    if (source_specified && !destination_specified) {
		pragma_output_directory = pragma_source_directory;
		printf("=> Using default output directory %s\n", pragma_output_directory);
		posts_output_directory = malloc(strlen(pragma_output_directory) + strlen(SITE_POSTS) + 64); 
		if (posts_output_directory == NULL ) {
			printf("! Error: can't allocate memory for string representing the output path...\n");
			exit(EXIT_FAILURE);
		}
		// Figure out the posts directory, given the site base output dir
		strcpy(posts_output_directory, pragma_output_directory);
		strcat(posts_output_directory, pragma_output_directory[strlen(pragma_output_directory)-1] != '/' ? "/" : "");
		strcat(posts_output_directory, SITE_POSTS);
	}

	site_info* config = load_site_yaml(pragma_source_directory);
	
	// If this evaluates to true, something went wrong above--no config or memory/file permission problems
	if (config == NULL) {
		printf("! Error: Can't proceed without site configuration! Aborting.\n");
		exit(EXIT_FAILURE);
	}

	wcscpy(config->base_dir, wchar_convert(pragma_source_directory));
	wprintf(L"%s\n", config->base_dir);

	// Load the site sources from the specified directory
	printf("base: %s\n", pragma_source_directory);
	pp_page* page_list = load_site( 0, pragma_source_directory );	
	char *icons_directory = char_convert(config->icons_dir); 

	// load the list of site icons and store them in config->site_icons; assign them
	load_site_icons(pragma_source_directory, icons_directory, config);
	assign_icons(page_list, config);

	// Ensure that the list of sources is sorted by date, with the newest content first
	sort_site(&page_list);

	// Process those sources: parse the markdown content...
	parse_site_markdown(page_list);

	// ...and build the indices
	int num_pages = 0;
	for (pp_page *y = page_list ; y->next != NULL ; y = y->next ) {
		num_pages++;
	} 
	int num_indices = ((num_pages + 1) / config->index_size) + 1;
	printf("=> processed %d page sources; will generate %d indices.\n", num_pages == 0 ? 0 : num_pages + 1, num_indices);

	wchar_t *main_index;
	char index_name[4];

	for (int i = 0 ; i < num_indices ; i++ ) {
		printf("index build loop entered\n");
		main_index = build_index(page_list, config, i);	
		printf("passed build_index() call\n");
		snprintf(index_name, 4, "%d", i);
		printf("%s", index_name);
		
		// write indices to diskA
		printf("allocating space for destination filename:\n");
		char *index_destination = malloc(strlen(pragma_output_directory) + 20);
		printf("passed malloc() for destination filename\n");
		
		strcpy(index_destination, pragma_output_directory);
		strcat(index_destination, "index");
		strcat(index_destination, i > 0 ? index_name : ""); 
		strcat(index_destination, ".html");
		write_file_contents(index_destination, main_index);

		// clean up from index generation
		free(index_destination);
		free(main_index);
	}

	// generate the scroll
	wchar_t *main_scroll = build_scroll(page_list, config);
	printf("=> Generated scoll.\n");
	
	// write scroll to disk
	char *scroll_destination = malloc(strlen(pragma_output_directory) + 20);
	strcpy(scroll_destination, pragma_output_directory);
	strcat(scroll_destination, "s/index.html");
	write_file_contents(scroll_destination, main_scroll);
	
	printf("=> Scroll written successfully to disk.\n");
	free(scroll_destination);
	free(main_scroll);

	// generate tag index
	wchar_t *tag_index = build_tag_index(page_list, config);
	printf("=> Generated tag index.\n");

	char *tag_destination = malloc(strlen(pragma_output_directory) + 20);
	strcpy(tag_destination, pragma_output_directory);
	strcat(tag_destination, "t/index.html");
	write_file_contents(tag_destination, tag_index);

	printf("=> Tag index successfully written to disk.\n");
	free(tag_destination);
	free(tag_index);
	
	char *destination_file;

	// We've generated a site at this point: write it to disk
	for (pp_page *current = page_list; current != NULL; current = current->next) {

		// First, assemble the required filename
		destination_file = malloc(256);		
		strcpy(destination_file, posts_output_directory);
		wchar_t *d = string_from_int(current->date_stamp);
		char *ds = char_convert(d);
		strcat(destination_file, ds);
		free(ds); free(d);
		strip_terminal_newline(NULL, destination_file);
		strcat(destination_file, ".html");

		// Then call build_single-page() to convert the current page to HTML...
		wchar_t *the_page = build_single_page(current, config);
		// ...and write the output to disk, freeing memory we had to allocate.
		write_file_contents(destination_file, the_page);
		free(the_page);
		free(destination_file);
	}

	// clean up from site generation
	wprintf(L"Generated site output. Cleaning up...\n");
	free_page_list(page_list);
	free(posts_output_directory);
	free_site_info(config);
	wprintf(L"Done.\n");
}
