#include "pragma_poison.h"

/**
 * write_file_contents(): Write wide-character content to a file path.
 *
 * Opens `path` for writing (truncating if it exists) and writes `content` via fwprintf().
 *
 * arguments:
 *  const char    *path    (filesystem path to write; must not be NULL)
 *  const wchar_t *content (null-terminated wide string to write; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on error)
 */
int write_file_contents(const utf8_path path, const wchar_t *content) {
	FILE *file = utf8_fopen(path, "w");

	if (file == NULL) {
		perror("! Unable to open file for writing in write_file_contents()!");
		return -1; // If we aren't able to open the file, bail and return an error
	}

	// Convert wide characters to UTF-8 and write as bytes

	// Find where the invalid character might be
	size_t test_len = wcstombs(NULL, content, 0);
	if (test_len == (size_t)-1) {
		// Invalid wide character found, but continue without debug output
	}

	char *utf8_content = char_convert(content);
	if (!utf8_content) {
		fclose(file);
		printf("! Unable to convert content to UTF-8 in write_file_contents() for path: %s\n", path);
		perror("char_convert failed");
		return -1;
	}

	//wprintf(L"%ls", content);

	int written = fprintf(file, "%s", utf8_content);
	free(utf8_content);
	fclose(file);

	if (written < 0) { // <= -1 bytes written...aka something went wrong
		perror("! Unable to write to file in write_file_contents()!");
		return -1;
	}
   	return 0; // Return 0 for success
}

/**
 * read_file_contents(): Read a file into a newly allocated wide-character buffer.
 *
 * Reads the file as wide characters with fgetwc() and returns a heap-allocated buffer.
 * Caller owns the returned memory and must free().
 *
 * arguments:
 *  const char *path (filesystem path to read; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated buffer containing the file contents; NULL on error)
 */
wchar_t* read_file_contents(const utf8_path path) {
	FILE *file = utf8_fopen(path, "r");

	if (!file) {
		perror("Error opening file");
		return NULL;
	}

	wint_t ch;
	wchar_t *content = NULL;
	size_t contentSize = 0;
	size_t index = 0;

	while ((ch = fgetwc(file)) != WEOF) {
		if (index == contentSize) {
			// Expand the buffer if needed
			contentSize += 10;
			wchar_t *temp = realloc(content, contentSize * sizeof(wchar_t));
			if (!temp) {
				perror("realloc()");
				free(content);
				fclose(file);
				return NULL;
			}
			content = temp;
		}
		content[index++] = ch;
	}

	if (index < contentSize) {
		content[index] = L'\0';
	} else {
		wchar_t *temp = realloc(content, (contentSize + 1) * sizeof(wchar_t));
		if (!temp) {
			perror("realloc()");
			free(content);
			fclose(file);
			return NULL;
		}
		content = temp;
		content[index] = L'\0';
	}

	// Close the file
	fclose(file);
	return content;
}

/**
 * UTF-8 safe filesystem wrapper functions
 *
 * These functions ensure proper UTF-8 handling for filesystem operations
 * by validating that paths are properly encoded UTF-8 strings.
 */

/**
 * utf8_fopen(): UTF-8 safe wrapper for fopen().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded file path; must not be NULL)
 *  const char* mode (file access mode; must not be NULL)
 *
 * returns:
 *  FILE* (file pointer on success; NULL on failure)
 */
FILE* utf8_fopen(const utf8_path path, const char* mode) {
    if (!path || !mode) {
        return NULL;
    }

    // On Unix systems with proper locale, standard fopen handles UTF-8
    return fopen(path, mode);
}

/**
 * utf8_opendir(): UTF-8 safe wrapper for opendir().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded directory path; must not be NULL)
 *
 * returns:
 *  DIR* (directory pointer on success; NULL on failure)
 */
DIR* utf8_opendir(const utf8_path path) {
    if (!path) {
        return NULL;
    }

    return opendir(path);
}

/**
 * utf8_stat(): UTF-8 safe wrapper for stat().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded file path; must not be NULL)
 *  struct stat* buf (stat buffer; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int utf8_stat(const utf8_path path, struct stat* buf) {
    if (!path || !buf) {
        return -1;
    }

    return stat(path, buf);
}

/**
 * utf8_mkdir(): UTF-8 safe wrapper for mkdir().
 *
 * arguments:
 *  const utf8_path path (UTF-8 encoded directory path; must not be NULL)
 *  mode_t mode (directory permissions)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int utf8_mkdir(const utf8_path path, mode_t mode) {
    if (!path) {
        return -1;
    }

    return mkdir(path, mode);
}

/**
 * wchar_to_utf8(): Convert wide string to UTF-8 path.
 *
 * Wrapper around char_convert() with explicit UTF-8 semantics.
 *
 * arguments:
 *  const wchar_t *wide_str (source wide string; must not be NULL)
 *
 * returns:
 *  utf8_path (heap-allocated UTF-8 string; NULL on error)
 */
utf8_path wchar_to_utf8(const wchar_t* wide_str) {
    if (!wide_str) {
        return NULL;
    }

    // char_convert uses wcstombs which respects UTF-8 locale
    return char_convert(wide_str);
}

/**
 * utf8_to_wchar(): Convert UTF-8 path to wide string.
 *
 * Wrapper around wchar_convert() with explicit UTF-8 semantics.
 *
 * arguments:
 *  const utf8_path utf8_str (source UTF-8 string; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated wide string; NULL on error)
 */
wchar_t* utf8_to_wchar(const utf8_path utf8_str) {
    if (!utf8_str) {
        return NULL;
    }

    // wchar_convert uses mbstowcs which respects UTF-8 locale
    return wchar_convert(utf8_str);
}

/**
 * get_last_run_time(): Read the last successful run timestamp from pragma_last_run.yml
 *
 * Reads the timestamp from the pragma_last_run.yml file in the site directory.
 * Returns 0 if the file doesn't exist or can't be read (meaning never run before).
 *
 * arguments:
 *  const char *site_directory (site root directory; must not be NULL)
 *
 * returns:
 *  time_t (last run timestamp, or 0 if never run/error)
 */
time_t get_last_run_time(const char *site_directory) {
    if (!site_directory) {
        return 0;
    }

    char *last_run_path = malloc(strlen(site_directory) + 32);
    if (!last_run_path) {
        return 0;
    }

    strcpy(last_run_path, site_directory);
    if (site_directory[strlen(site_directory)-1] != '/') {
        strcat(last_run_path, "/");
    }
    strcat(last_run_path, "pragma_last_run.yml");

    FILE *file = utf8_fopen(last_run_path, "r");
    free(last_run_path);

    if (!file) {
        return 0; // File doesn't exist, treat as never run
    }

    char line[256];
    time_t last_run = 0;

    if (fgets(line, sizeof(line), file)) {
        // Parse the timestamp from the file
        last_run = (time_t)strtol(line, NULL, 10);
    }

    fclose(file);
    return last_run;
}

/**
 * update_last_run_time(): Write the current timestamp to pragma_last_run.yml
 *
 * Updates the pragma_last_run.yml file with the current timestamp to mark
 * a successful site generation run.
 *
 * arguments:
 *  const char *site_directory (site root directory; must not be NULL)
 *
 * returns:
 *  void
 */
void update_last_run_time(const char *site_directory) {
    if (!site_directory) {
        return;
    }

    char *last_run_path = malloc(strlen(site_directory) + 32);
    if (!last_run_path) {
        return;
    }

    strcpy(last_run_path, site_directory);
    if (site_directory[strlen(site_directory)-1] != '/') {
        strcat(last_run_path, "/");
    }
    strcat(last_run_path, "pragma_last_run.yml");

    FILE *file = utf8_fopen(last_run_path, "w");
    free(last_run_path);

    if (!file) {
        return; // Can't write, silently fail
    }

    time_t now = time(NULL);
    fprintf(file, "%ld\n", (long)now);
    fclose(file);
}

/**
 * is_pragma_generated(): Check if an HTML file was generated by pragma-web.
 *
 * Opens the file and looks for the generator meta tag.
 *
 * arguments:
 *  const char *file_path (path to HTML file to check)
 *
 * returns:
 *  bool (true if file contains pragma-web generator tag, false otherwise)
 */
bool is_pragma_generated(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        return false;
    }

    char line[1024];
    bool found_generator = false;

    // Look for the generator meta tag in the first portion of the file
    while (fgets(line, sizeof(line), file) && !found_generator) {
        if (strstr(line, "pragma-web")) {
            found_generator = true;
            break;
        }
        // Stop searching after we've gone past the head section
        if (strstr(line, "</head>") || strstr(line, "<body")) {
            break;
        }
    }

    fclose(file);
    return found_generator;
}

/**
 * source_file_exists(): Check if a corresponding source file exists for an HTML file.
 *
 * Given an HTML filename like "test.html", checks if "test.txt" exists in the source directory.
 *
 * arguments:
 *  const char *html_filename (just the filename, not full path)
 *  const char *source_dir (source directory to check in)
 *
 * returns:
 *  bool (true if corresponding source file exists)
 */
bool source_file_exists(const char *html_filename, const char *source_dir) {
    // Extract base name without .html extension
    char *base_name = strdup(html_filename);
    if (!base_name) return false;

    char *dot = strrchr(base_name, '.');
    if (dot && strcmp(dot, ".html") == 0) {
        *dot = '\0';  // Remove .html extension
    }

    // Build path to corresponding .txt file in dat/ subdirectory
    char source_path[1024];
    snprintf(source_path, sizeof(source_path), "%s/dat/%s.txt", source_dir, base_name);

    free(base_name);

    // Check if file exists
    FILE *file = fopen(source_path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/**
 * cleanup_stale_files(): Remove pragma-generated HTML files that no longer have source files.
 *
 * Scans the output directory's /c/ folder for HTML files, checks if they were generated
 * by pragma-web, and if their corresponding source files no longer exist, asks the user
 * if they want to delete them.
 *
 * arguments:
 *  const char *source_dir (source directory containing dat/ folder)
 *  const char *output_dir (output directory containing c/ folder)
 *
 * returns:
 *  void
 */
void cleanup_stale_files(const char *source_dir, const char *output_dir) {
    // Build path to posts directory (c/)
    char posts_dir[1024];
    snprintf(posts_dir, sizeof(posts_dir), "%s/c", output_dir);

    DIR *dir = opendir(posts_dir);
    if (!dir) {
        printf("=> Could not open posts directory %s for stale file cleanup\n", posts_dir);
        return;
    }

    // Collect stale files
    char stale_files[100][256];  // Array to store stale file names
    int stale_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && stale_count < 100) {
        // Skip non-HTML files and directories
        if (entry->d_type != DT_REG) continue;

        char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".html") != 0) continue;

        // Build full path to HTML file
        char html_path[1024];
        snprintf(html_path, sizeof(html_path), "%s/%s", posts_dir, entry->d_name);

        // Check if it's pragma-generated
        if (!is_pragma_generated(html_path)) {
            continue;  // Skip manually created files
        }

        // Check if corresponding source file exists
        if (!source_file_exists(entry->d_name, source_dir)) {
            strncpy(stale_files[stale_count], entry->d_name, sizeof(stale_files[0]) - 1);
            stale_files[stale_count][sizeof(stale_files[0]) - 1] = '\0';
            stale_count++;
        }
    }
    closedir(dir);

    // If no stale files found, report and return
    if (stale_count == 0) {
        printf("=> No stale pragma-generated files found\n");
        return;
    }

    // Show stale files and ask for confirmation
    printf("=> Found %d stale pragma-generated file%s:\n", stale_count, stale_count == 1 ? "" : "s");
    for (int i = 0; i < stale_count; i++) {
        printf("  - c/%s (no corresponding dat/ source file)\n", stale_files[i]);
    }

    printf("\nDelete these stale files? [y/N]: ");
    fflush(stdout);

    char response[10];
    if (fgets(response, sizeof(response), stdin)) {
        if (response[0] == 'y' || response[0] == 'Y') {
            // Delete the files
            int deleted = 0;
            for (int i = 0; i < stale_count; i++) {
                char file_path[1024];
                snprintf(file_path, sizeof(file_path), "%s/%s", posts_dir, stale_files[i]);

                if (remove(file_path) == 0) {
                    printf("  ✓ Deleted %s\n", stale_files[i]);
                    deleted++;
                } else {
                    printf("  ✗ Failed to delete %s\n", stale_files[i]);
                }
            }
            printf("=> Deleted %d of %d stale files\n", deleted, stale_count);
        } else {
            printf("=> Stale file cleanup cancelled\n");
        }
    } else {
        printf("=> Stale file cleanup cancelled\n");
    }
}