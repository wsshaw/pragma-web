#include "pragma_poison.h"

/**
 * check_dir(): Verify that a directory exists and has the requested permission bit.
 *
 * Checks that `p` exists, is a directory, and its mode includes the specified `mode`
 *
 * arguments:
 *  const char *p (path to test; must not be NULL)
 *  int         mode (permission bit to test for, like S_IRUSR or S_IWUSR)
 *
 * returns:
 *  bool (true if directory exists and has requested permission; false otherwise) 
 */
bool check_dir( const utf8_path p, int mode ) {
	struct stat dir;
	return (utf8_stat(p, &dir) == 0 && S_ISDIR(dir.st_mode) && dir.st_mode & mode);
}

/**
 * usage(): Print the CLI usage string.
 *
 * Writes PRAGMA_USAGE (defined in headers) to stdout.
 *
 * arguments:
 *  void
 *
 * returns:
 *  void */
void usage() {
	printf("%s", PRAGMA_USAGE);
}

/**
 * is_valid_site(): Quick check that a candidate site directory is readable and exists. 
 *
 * Currently tests only directory existence + read bit. Future versions should validate structure
 *
 * arguments:
 *  const char *path (directory to test; must not be NULL)
 *
 * returns:
 *  bool (true if readable directory; false otherwise)
 */
bool is_valid_site(const utf8_path path) {
	// Can we even read the directory?
	if (!check_dir(path,S_IRUSR))
		return false;
	return true;
}

/**
 * build_url(): Construct a URL by safely joining a base URL with a path.
 *
 * Handles slash normalization between base_url and path to avoid issues like
 * "http://example.compath" or "http://example.com//path". Allocates memory
 * that must be freed by the caller.
 *
 * arguments:
 *  const wchar_t *base_url (base URL; must not be NULL)
 *  const wchar_t *path (path to append; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated complete URL; NULL on error)
 */
wchar_t* build_url(const wchar_t *base_url, const wchar_t *path) {
	if (!base_url || !path) return NULL;

	size_t base_len = wcslen(base_url);
	size_t path_len = wcslen(path);

	// Allocate space for base + "/" + path + null terminator
	wchar_t *url = malloc((base_len + path_len + 2) * sizeof(wchar_t));
	if (!url) return NULL;

	// Copy base URL
	wcscpy(url, base_url);

	// Check if we need to add a slash between base and path
	bool base_has_slash = (base_len > 0 && base_url[base_len - 1] == L'/');
	bool path_has_slash = (path_len > 0 && path[0] == L'/');

	if (!base_has_slash && !path_has_slash) {
		// Need to add slash: "http://example.com" + "path" -> "http://example.com/path"
		wcscat(url, L"/");
	} else if (base_has_slash && path_has_slash) {
		// Remove one slash: "http://example.com/" + "/path" -> "http://example.com/path"
		path++; // Skip the leading slash in path
	}
	// Otherwise: one has slash, one doesn't - perfect as-is

	wcscat(url, path);
	return url;
}

/**
 * legible_date(): Convert an epoch timestamp to a formatted wide-character date string.
 *
 * Uses localtime() and wcsftime() with the format "%Y-%m-%d %H:%M:%S".
 * Caller must free the returned buffer.
 *
 * arguments:
 *  time_t when (epoch time)
 *
 * returns:
 *  wchar_t* (heap-allocated date string)
 */
wchar_t* legible_date(time_t when) {
	struct tm t;
	wchar_t *output = malloc(64 * sizeof(wchar_t));

	t = *localtime(&when);
	wcsftime(output, 64, L"%Y-%m-%d %H:%M:%S", &t);
	return output;
}
