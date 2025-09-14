#include "pragma_poison.h"

/**
* Array of important directories for the software. (When you create a new site, we iterate over
* this array and create the directories, so there's no key, like an enum of names, in header files)
*/
const char *pragma_directories[] = {
	"/dat/",
	"/img/",
	"/img/icons/",
	"/a/",
	"/c/",
	"/t/",
	"/s/",
	"/templates/"
};

/**
* Basic configuration, JavaScript, CSS, and HTML files that must be created by the software when
* we generate a new site. As with the above array, there's no need to address these items in a
* semantically legible way; we just need to make sure the files exist.
*
* That said, the enum below (pragma_file_info) provides a friendly way to address the elements.
*/
const char *pragma_basic_files[] = {
	"/pragma_config.yml",
	"/pragma_last_run.yml",
	"/p.css",
	"/j.js",
	"/a/index.html",
	"/_header.html",
	"/_footer.html",
	"/templates/post_card.html",
	"/templates/single_page.html",
	"/templates/navigation.html",
	"/templates/index_item.html",
	"/dat/sample_post.txt",
	"/img/icons/default.svg"
};

/**
* Contents of the files above, as defined in the headers -- placeholder strings or default values.
*/
const wchar_t *pragma_basic_file_skeletons[] = {
	DEFAULT_YAML,
	L"0",		// default last run time: forever ago
	DEFAULT_CSS,
	DEFAULT_JAVASCRIPT,
	DEFAULT_ABOUT_PAGE,
	DEFAULT_HEADER,
	DEFAULT_FOOTER,
	DEFAULT_TEMPLATE_POST_CARD,
	DEFAULT_TEMPLATE_SINGLE_PAGE,
	DEFAULT_TEMPLATE_NAVIGATION,
	DEFAULT_TEMPLATE_INDEX_ITEM,
	DEFAULT_SAMPLE_POST,
	DEFAULT_ICON_SVG
};

/**
* A convenience enum of legible ways to refer to items in the pragma_basic_files[] array.
*/
enum pragma_file_info {
	CONFIGURATION,
	LAST_RUN,
	CSS,
	JAVASCRIPT,
	ABOUT,
	HEADER,
	FOOTER,
	TEMPLATE_POST_CARD,
	TEMPLATE_SINGLE_PAGE,
	TEMPLATE_NAVIGATION,
	TEMPLATE_INDEX_ITEM,
	SAMPLE_POST,
	DEFAULT_ICON
};

/**
 * build_new_pragma_site(): Initialize a new site in the target directory.
 *
 * Creates required subdirectories, writes baseline config/header/footer/JS/CSS files,
 * and reports progress to stdout. Aborts on the first failure.
 *
 * arguments:
 *  char *t (absolute or working-directory-relative path to the site root; must be writable)
 *
 * returns:
 *  void
*/
void build_new_pragma_site( char *t ) {
	int status;
	char path[256];

	// Ideally, this has happened before calling this function, but let's be sure
	if (!check_dir( t, S_IWUSR )) {
		printf("! Error in build_new_pragma_site(): directory not writable (%s)", t);
		return;
	}

	// Create the necessary subdirectories
	printf("=> Making site subdirectories.\n");

	for (int i = 0 ; i < (int)(sizeof(pragma_directories) / sizeof(pragma_directories[0])) ; ++i) {
		strcpy(path,t);
		strcat(path, pragma_directories[i]);

		status = utf8_mkdir(path, 0700);
		if (status == 0) {
			printf("   => Created directory %s\n", path);
		} else {
			printf("! Error: couldn't create directory %s!\n", pragma_directories[i]);
			perror("Aborting. The source directories for this website won't be properly configured.\n");
			return;
		}
	}

	// If we made it this far, the directories have to exist; create the skeleton files
	printf("=> Making site config, templates, and examples.\n");
	for (int i = 0 ; i < (int)(sizeof(pragma_basic_files) / sizeof(pragma_basic_files[0])) ; ++i) {
		strcpy(path,t);
		strcat(path, pragma_basic_files[i]);

		status = write_file_contents(path, pragma_basic_file_skeletons[i]);
		if (status == 0)
			printf("   => Created %s.\n", pragma_basic_files[i]);
		else {
			printf("! Error: couldn't create file %s!\n", pragma_basic_files[i]);
			perror("failure: ");
			return;
		}

	}
	printf("Successfully created new site in %s.\nYou should edit %s/pragma_config.yml before building the site.\n\n", t, t);
}