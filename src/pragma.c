#include "pragma_poison.h"

/**
 * cleanup_buffer_pool(): Clean up global buffer pool at program exit.
 */
void cleanup_buffer_pool(void) {
	buffer_pool_cleanup_global();
}

/**
 * parse_arguments(): Parse command-line arguments using getopt.
 *
 * arguments:
 *  int argc (argument count)
 *  char *argv[] (argument vector)
 *  pragma_options *opts (options structure to populate)
 *
 * returns:
 *  int (0 on success, -1 on error)
 */
int parse_arguments(int argc, char *argv[], pragma_options *opts) {
    int option;

    // Initialize options to defaults
    memset(opts, 0, sizeof(pragma_options));

    // Parse options using getopt
    while ((option = getopt(argc, argv, "s:o:c:funhdx")) != -1) {
        switch (option) {
            case 's':
                opts->source_dir = optarg;
                break;
            case 'o':
                opts->output_dir = optarg;
                break;
            case 'c':
                opts->create_site = true;
                opts->output_dir = optarg; // -c takes a directory argument
                break;
            case 'f':
                opts->force_all = true;
                break;
            case 'u':
                opts->updated_only = true;
                break;
            case 'n':
                opts->new_only = true;
                break;
            case 'd':
                opts->dry_run = true;
                break;
            case 'h':
                opts->show_help = true;
                break;
            case 'x':
                opts->clean_stale = true;
                break;
            case '?':
                // getopt prints error message for unknown options
                return -1;
            default:
                return -1;
        }
    }

    return 0;
}

/**
 * validate_options(): Validate parsed command-line options.
 *
 * arguments:
 *  pragma_options *opts (parsed options)
 *
 * returns:
 *  int (0 if valid, -1 if invalid, 1 if we just show usage and exit, 2 for create site,
 * which really needs to be documented somewhere and not use magic numbers) 
 */
int validate_options(pragma_options *opts) {
    // Handle immediate actions first
    if (opts->show_help) {
        usage();
        return 1; // user requested -h (help)
    }

    if (opts->create_site) {
        if (!opts->output_dir) {
            printf("Error: -c requires a directory argument\n");
            printf("Usage: pragma -c [destination directory]\n");
            return -1;
        }
        // Validate directory exists and is writable
        if (!check_dir(opts->output_dir, S_IWUSR)) {
            printf("Error: directory '%s' does not exist or is not writable\n", opts->output_dir);
            printf("Please create the directory and/or adjust permissions\n");
            return -1;
        }
        return 2; // Special return code for create site (should exit after creation)
    }

    // For normal operation, we need both source and output
    if (!opts->source_dir) {
        printf("Error: must specify source directory with -s\n");
        printf("       or create new site with -c [directory]\n");
        usage();
        return -1;
    }

    if (!opts->output_dir) {
        printf("Error: must specify output directory with -o\n");
        printf("       (or -c to create new site)\n");
        return -1;
    }

	/** In general, pragma is not going to create directories: need to improve messaging and
	 * write some documentation around this fact, but it won't try to diagnose permissions issues etc.
	 */
    // Validate source directory
    if (!check_dir(opts->source_dir, S_IRUSR)) {
        printf("Error: source directory '%s' does not exist or is not readable\n", opts->source_dir);
        printf("To create a new site, use: pragma -c [directory]\n");
        return -1;
    }

    // Validate output directory
    if (!check_dir(opts->output_dir, S_IWUSR)) {
        printf("Error: output directory '%s' does not exist or is not writable\n", opts->output_dir);
        return -1;
    }

    // Validate conflicting options instead of just blasting on through with -f 
    if (opts->force_all && opts->updated_only) {
        printf("Error: cannot specify both -f (force all) and -u (updated only)\n");
        return -1;
    }

    // same with -u / -n, though it's a little less egregious 
    if (opts->updated_only && opts->new_only) {
        printf("Error: cannot specify both -u (updated only) and -n (new only)\n");
        return -1;
    }

    return 0; // Valid for normal operation
}

/**
 * main(): Entry point for the `pragma web` static site generator.
 *
 * Parses CLI arguments using getopt, validates options, and dispatches
 * site-building operations.
 *
 * arguments:
 *  int   argc (number of command-line arguments)
 *  char *argv[] (array of argument strings)
 *
 * returns:
 *  int (EXIT_SUCCESS or EXIT_FAILURE)
 */
int main(int argc, char *argv[]) {
	// setlocale() needs to be called asap so that Unicode is handled properly
	setlocale(LC_CTYPE, "en_US.UTF-8");

	// Initialize global buffer pool for unified buffer management
	buffer_pool_init_global();

	// Register cleanup function for automatic cleanup at exit
	atexit(cleanup_buffer_pool);

    pragma_options opts;
    char *posts_output_directory;

	// Show usage if no arguments provided
	if (argc == 1) {
		usage();
		exit(EXIT_FAILURE);
	}

    // Parse command-line arguments
    if (parse_arguments(argc, argv, &opts) != 0) {
        exit(EXIT_FAILURE);
    }

    // Validate options and handle immediate actions
    int validation_result = validate_options(&opts);
    if (validation_result == -1) {
        exit(EXIT_FAILURE);
    } else if (validation_result == 1) {
        // Help was shown
        exit(EXIT_SUCCESS);
    } else if (validation_result == 2) {
        // Create site and exit
        printf("Will create a new pragma-web site in %s\n\n", opts.output_dir);
        build_new_pragma_site(opts.output_dir);
        exit(EXIT_SUCCESS);
    }

    // Initialize logging system
    log_level_t log_level = LOG_INFO;  // Default to info level
    bool quiet_mode = opts.dry_run;    // Quiet mode for dry runs
    log_init(log_level, quiet_mode);

    // At this point we have valid source and output directories
    log_info("Using source directory %s", opts.source_dir);
    log_info("Using output directory %s", opts.output_dir);

    // Set up posts output directory
    posts_output_directory = malloc(strlen(opts.output_dir) + strlen(SITE_POSTS) + 64);
    if (posts_output_directory == NULL) {
        log_fatal("can't allocate memory for posts output path");
        exit(EXIT_FAILURE);
    }

    // Build posts directory path
    strcpy(posts_output_directory, opts.output_dir);
	// accommodate trailing / or not
    strcat(posts_output_directory, opts.output_dir[strlen(opts.output_dir)-1] != '/' ? "/" : "");
    strcat(posts_output_directory, SITE_POSTS);

    // Load site configuration
    site_info* config = load_site_yaml(opts.source_dir);
    if (config == NULL) {
        log_fatal("Can't proceed without site configuration! Aborting.");
        free(posts_output_directory);
        exit(EXIT_FAILURE);
    }

    wcscpy(config->base_dir, wchar_convert(opts.source_dir));
    // wprintf(L"%s\n", config->base_dir); ??? old debugging?

    // Determine loading mode based on options
    int load_mode = LOAD_EVERYTHING; // default to full site load
    time_t since_time = 0;

    if (opts.updated_only) {
        load_mode = LOAD_UPDATED_ONLY;
        since_time = get_last_run_time(opts.source_dir);
        log_info("Loading files updated since last run");
    } else if (opts.new_only) {
        // TODO: Implement new-only mode
        log_info("New-only mode not yet implemented, loading everything");
    } else if (opts.force_all) {
        log_info("Force rebuilding all files");
    }

    if (opts.dry_run) {
        log_info("DRY RUN MODE: No files will be written");
    }

	log_debug("load = %d", load_mode);
    // Load the site sources
    pp_page* pages = load_site(load_mode, opts.source_dir, since_time);

    if (pages == NULL) {
        log_error("no pages found or loaded");
        free(posts_output_directory);
        free_site_info(config);
        exit(EXIT_FAILURE);
    }

    // Process markdown content
    parse_site_markdown(pages);

    // Sort pages by date
    sort_site(&pages);

    // Load site icons
    load_site_icons(opts.output_dir, char_convert(config->icons_dir), config);

    // Assign icons to pages
    assign_icons(pages, config, opts.source_dir);

    // Build the site (unless dry run)
    if (!opts.dry_run) {
        // Build individual pages
        pp_page *current_page = pages;
        int page_count = 0;
        while (current_page != NULL) {
            log_info("Building page %d: %ls (tags: %ls)", ++page_count,
                   current_page->title ? current_page->title : L"[no title]",
                   current_page->tags ? current_page->tags : L"[no tags]");
            wchar_t *page_html = build_single_page(current_page, config);
            if (page_html) {
                write_single_page(current_page, posts_output_directory, page_html);
                free(page_html);
            } else {
                log_error("build_single_page returned NULL for page %d", page_count);
            }
            current_page = current_page->next;
        }
        log_info("Built %d individual pages.", page_count);
        // Build index pages
        if (config->index_size > 0) {
            // Count total pages to determine how many index pages we need
            int total_posts = 0;
            for (pp_page *count_page = pages; count_page != NULL; count_page = count_page->next) {
                total_posts++;
            }

            int total_index_pages = (total_posts + config->index_size - 1) / config->index_size; // Ceiling division
            log_info("building %d index pages for %d posts...", total_index_pages, total_posts);

            for (int page_num = 0; page_num < total_index_pages; page_num++) {
                wchar_t *index_html = build_index(pages, config, page_num);
                if (index_html) {
                    char index_path[1024];
                    if (page_num == 0) {
                        // First page is index.html
                        snprintf(index_path, sizeof(index_path), "%s/index.html", opts.output_dir);
                    } else {
                        // Subsequent pages are index1.html, index2.html, etc.
                        snprintf(index_path, sizeof(index_path), "%s/index%d.html", opts.output_dir, page_num);
                    }
                    write_file_contents(index_path, index_html);
                    free(index_html);
                }
            }
        }

        // Build scroll (chronological index)
        if (config->build_scroll) {
			log_info("building scroll...");
            wchar_t *scroll_html = build_scroll(pages, config);
            if (scroll_html) {
                char scroll_path[1024];
                snprintf(scroll_path, sizeof(scroll_path), "%s/s/index.html", opts.output_dir);
                write_file_contents(scroll_path, scroll_html);
                free(scroll_html);
            }
        }

        // Build tag indices
        if (config->build_tags) {
			log_info("building tag indices...");
            wchar_t *tag_html = build_tag_index(pages, config);
            if (tag_html) {
                char tag_path[1024];
                snprintf(tag_path, sizeof(tag_path), "%s/t/index.html", opts.output_dir);
                write_file_contents(tag_path, tag_html);
                free(tag_html);
            }
        }

        // Build RSS feed
		log_info("generating RSS feed...");
        wchar_t *rss_xml = build_rss(pages, config);
        if (rss_xml) {
            char rss_path[1024];
            snprintf(rss_path, sizeof(rss_path), "%s/feed.xml", opts.output_dir);
            write_file_contents(rss_path, rss_xml);
            free(rss_xml);
        }

        // Update last run time
        update_last_run_time(opts.source_dir);

        log_info("Site generation complete.");

        // Clean up stale files if requested
        if (opts.clean_stale) {
            cleanup_stale_files(opts.source_dir, opts.output_dir);
        }
    } else {
        log_info("Dry run complete - no files written");
    }

    // Cleanup
    free(posts_output_directory);
    free_page_list(pages);
    free_site_info(config);

    exit(EXIT_SUCCESS);
}
