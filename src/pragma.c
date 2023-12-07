#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <stddef.h>

#include "pragma_poison.h"

int main(int argc, char *argv[]) {
	// setlocale() needs to be called here so that Unicode is handled properly
	setlocale(LC_CTYPE, "en_US.UTF-8");

	char *pragma_source_directory, *pragma_output_directory, *posts_output_directory;

	// some options we need to keep track of at startup
	bool source_specified = false;
	bool destination_specified = false;
	bool dry_run_specified = false;
	bool force_all_specified = false;
	bool updated_only = false;
	bool create_site_specified = false;
	
	if ( argc == 1 ) {
		usage();
		exit(EXIT_SUCCESS);
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
	// We must have either (-s && -o) || (-c)
	// if we have (-s && -o)
	//	source and output confirmed; check mode flag and act accordingly
	// if we have (-s || -o)
	//	warn user that this isn't gonna work, exit
	// else if we have (-c)
	//	create new site, exit

	// TODO: change the logic so that we assume
	if (source_specified && !destination_specified) {
		printf("=> Error: must specify output directory with -o\n");
		exit(EXIT_SUCCESS);
	} else if (destination_specified && !source_specified) {
		printf("=> Error: must specify source directory with -s\n");
		exit(EXIT_SUCCESS);
	} else if (!source_specified && !destination_specified && !create_site_specified) {
		printf("=> Error: must specify either source and output directories (-s, -o) or create new site (-c)\n");
		exit(EXIT_SUCCESS);
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
					posts_output_directory = malloc(strlen(pragma_output_directory) + strlen(SITE_POSTS) + 64); //save space forfilename FIXME
					if (posts_output_directory == NULL ) {
						printf("! Error: can't allocate memory for string representing the output path...\n");
						exit(EXIT_FAILURE);
					}
					
					// Figure out the posts directory, given the site base output dir
					strcpy(posts_output_directory, pragma_output_directory);
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
				printf("Not in first");
			}
			if (i + 1 < argc) {
				// Create a new site. We won't make a directory that doesn't exist, though.
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
		} else if (strcmp(argv[i], "-f") == 0) {
		} else if (strcmp(argv[i], "-s") == 0) {
		} else if (strcmp(argv[i], "-u") == 0) {
		} else if (strcmp(argv[i], "-n") == 0) {
		} else if (strcmp(argv[i], "-h") == 0) {
			usage();
			exit(EXIT_SUCCESS);
			} else {
			// :-D
			}

		}

	site_info* config = load_site_yaml(pragma_source_directory);
	if (config == NULL) {
		printf("! Error: Can't proceed without site configuration! Aborting.\n");
		exit(EXIT_FAILURE);
	}

	// FIXME: For testing/development purposes, all this stuff is here but it obviously needs to
	// be moved 

	// Load the site sources from the specified directory
	pp_page* page_list = load_site( 0, SITE_SOURCES );

	// Ensure that the list is sorted by date, with the newest content first
	sort_site(&page_list);

	// Process the site sources: parse the markdown content...
	parse_site_markdown(page_list);

	// ...and build the indices
	wchar_t *main_index = build_index(page_list, config, 0);
	// write it, etc.
	free(main_index);

	char *destination_file;

	for (pp_page *current = page_list; current != NULL; current = current->next) {
		destination_file = malloc(256);		// i.e., find a place for the output
		strcpy(destination_file, posts_output_directory);
		strcat(destination_file, char_convert(current->date));

		strip_terminal_newline(NULL, destination_file);

		strcat(destination_file, ".html");

		wprintf(L"Title: %ls\n", current->title);
		wprintf(L"Tags: %ls\n", current->tags);
		printf("\n"); 
		// Need to check the right subdirectory here
		//wprintf(L"%ls.\n", build_single_page(current));
		wchar_t *the_page = build_single_page(current, config);
		write_file_contents(destination_file, the_page);
		free(the_page);
		free(destination_file);
	}
	free(page_list);
	free(posts_output_directory);
	free(config);
}
