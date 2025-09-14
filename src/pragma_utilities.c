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
