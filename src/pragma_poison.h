#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif
#include <stdbool.h>
#include <wchar.h>
#include <time.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <time.h>

// UTF-8 string type for filesystem operations
typedef char* utf8_path;

// UTF-8 safe filesystem operations
FILE* utf8_fopen(const utf8_path path, const char* mode);
DIR* utf8_opendir(const utf8_path path);
int utf8_stat(const utf8_path path, struct stat* buf);
int utf8_mkdir(const utf8_path path, mode_t mode);

// UTF-8 string conversion functions
utf8_path wchar_to_utf8(const wchar_t* wide_str);
wchar_t* utf8_to_wchar(const utf8_path utf8_str);

#define MAX_LINE_LENGTH 4096			// ...

// Inputs
#define SITE_SOURCES	"/Users/will/pragma2/dat/"// Input directory - dat files to convert to HTML.  (Default value; specify actual in argv or config)
#define SITE_SOURCES_DEFAULT_SUBDIR	"dat/"

// Outputs
#define SITE_ROOT	"/Users/will/pp2/"	// Where to stage the generated site (Default value; specify actual in argv or config)
#define SITE_POSTS	"c/"			// Directory (within SITE_ROOT) for all generated posts
#define SITE_SCROLL	"s/"			// Directory (within SITE_ROOT) for the scroll, i.e. index of all posts by date
#define SITE_TAG_INDEX	"t/"			// Directory (within SITE_ROOT) for the indices of posts per tag
#define SITE_IMAGES	"img/"			// Directory (within SITE_ROOT) where images are located
#define SITE_ICONS	"img/icons/"		// Directory (within SITE_ROOT) where icons are located
#define SITE_DEFAULT_IMG "img/default.png"	// general fallback image for the site...favico ish
                
#define PRAGMA_DEBUG	0

#define PRAGMA_USAGE	"Usage: pragma -s [source] -o [output]\n\nwhere [source] is the site source directory and [output] is where you want the rendered site.\n\n\t-f: regenerate all html for all nodes\n\t-s: dry run, status report only\n\t-u: regenerate html only for nodes whose source was modified since last successful run\n\t-n: generate html output for new nodes (i.e., created since last run)\n\t-h: show this 'help'\n\nPlease see README.txt for usage details and examples.\n\n"

/**
* Default file contents when creating a new site.  Some of these strings are hideous, but I prefer
* to generate the files from this source rather than template files.  (The pragma executable should
* be able to live anywhere with no supporting files)
*/
#define DEFAULT_YAML	L"---\nsite_name:Web disaster\njs:no\nbuild_tags:yes\nbuild_scroll:yes\ncss:p.css\nheader:_header.html\nfooter:_footer.html\nindex_size:10\nicons_dir:img/icons\ntagline:Comparison is always true due to limited range of data type.\nread_more:-1\ndefault_image:/img/default.png\nbase_url:https://yourdomain.edu"
#define DEFAULT_YAML_FILENAME "pragma_config.yml"
#define DEFAULT_CSS	L"body {\n  margin-left:10em;\n  max-width:50%;\n}\n\n" \
L"h2 {\n margin-bottom:2px;\n}\n\n" \
L"div.post_title h3 {\n  margin-bottom:2px;\n  margin-top:0px;\n}\n\n" \
L".icon {\n  float:left;\n  width:64px;\n  height:64px;\n  padding-right:10px;\n}\n\n" \
L"img.post {\n  display: block;\n  margin-left: auto;\n  margin-right: auto;\n  max-width: 75%;\n clear:both;\n}\n\n" \
L"div.post_head {\n  display:inline-block;\n  width:100%;\n  clear:both;\n}\n\n" \
L"div.foot {\nmargin: auto;\nwidth: 15%;\npadding-top: 1em;\nfont-size: larger;\n}\n\n" \
L"div.post_title {\n vertical-align:top;\n float:left;\n display:inline-block;\n" \
L"  overflow-wrap: break-word;\n  hyphens: auto;\n" \
L" width: -webkit-calc(100% - 80px);\n  width:    -moz-calc(100% - 80px);\n  width:         calc(100% - 80px);\n}\n\n" \
L"div.post_image_wrapper {\n  margin-left:auto;\n  margin-right:auto;\n  display:block;\n  clear:both;\n text-align:center;\n}\n" \
L"div.post_body {\n  float:left;\n}\n\n" \
L"@media only screen and (max-width: 600px) {\n  body{\n  margin-left: 1em;\n  margin-right: 1em;\n  max-width: 100%;\n  }\n" \
L"  img.post {\n   display:block;\n        margin-left:auto;\n        margin-right:auto;\n        max-width:100%;\n        clear:both;\n  }\n}\n" \
L"blockquote {\n  border:1px solid gray;\n  background-color:#eeeeee;\n  margin-left:5em;\n  margin-right:5em;\n  padding:1em;\n}"
#define DEFAULT_JAVASCRIPT	L""
#define DEFAULT_ABOUT_PAGE	L"[about page here]"

#define DEFAULT_HEADER		L"<!DOCTYPE html>"\
L"<html lang=\"en\" prefix=\"og: https://ogp.me/ns#\">"\
L"<head>"\
L"<meta charset=\"utf-8\">"\
L"<meta name=\"generator\" content=\"pragma_poison_1.0.0\">"\
L"<meta property=\"og:title\" content=\"{TITLE}\">"\
L"<meta property=\"og:type\" content=\"article\">"\
L"<meta property=\"og:locale\" content=\"en\">"\
L"<meta property=\"og:image\" content=\"{MAIN_IMAGE}\">"\
L"<meta property=\"og:site_name\" content=\"{SITE_NAME}\">"\
L"<meta property=\"og:url\" content=\"{PAGE_URL}\">"\
L"<title>#pragma poison | {PAGETITLE}</title>"\
L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"\
L"<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"\
L"<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"\
L"<link href=\"https://fonts.googleapis.com/css2?family=Merriweather:ital,opsz,wght@0,18..144,300..900;1,18..144,300..900&display=swap\" rel=\"stylesheet\">"\
L"</head>"\
L"<body>"\
L"<h1>#pragma poison</h1>"\
L"<p style=\"clear:none;\">"\
L"<span style=\"float:left;\">Comparison is always true due to limited range of data type</span>"\
L"<span style=\"float:right;\">{BACK}<a href=\"/\">front</a> | <a href=\"/s\">all</a> | <a href=\"/a\">about</a>{FORWARD}</span>"\
L"&nbsp;"\
L"</p>"\
L"<hr>"\
L"<div class=\"main_body\">"\
L"{TITLE}"\
L"{DATE}"\
L"{TAGS}"\
L"<p>"

#define DEFAULT_FOOTER	L"</div></body><script src=\"https://cdn.jsdelivr.net/npm/glightbox/dist/js/glightbox.min.js\"></script><link href=\"https://cdn.jsdelivr.net/npm/glightbox/dist/css/glightbox.min.css\" rel=\"stylesheet\"><script>const lightbox = GLightbox();</script>"\
L"<link rel=\"stylesheet\" href=\"/p.css\"></html>"\

#define LOAD_EVERYTHING		0
#define LOAD_METADATA		1
#define LOAD_FILENAMES_ONLY	2

#define MAX_MONTHLY_POSTS	128

#define EMPTY_ARRAY_FLAG	(1 << 0)

#define SIZE_OF(array) ( sizeof((array)) / sizeof((array[0])) )

#define READ_MORE_DELIMITER	L"#MORE"

extern const char *pragma_directories[];
extern const char *pragma_basic_files[];

typedef struct site_info {
	wchar_t *site_name;
	wchar_t *default_image;
	wchar_t *base_url;
	bool include_js;
	bool build_tags;
	bool build_scroll;
	wchar_t *css;
	wchar_t *js;
	wchar_t *header;
	wchar_t *footer;
	int index_size;
	int read_more;
	wchar_t *tagline;	
	wchar_t *icons_dir;
	wchar_t *base_dir;
	char **icons;
	int icon_sentinel;
} site_info;
     
struct pp_page;	// (forward declaration)
// Basic data type for holding page information 
typedef struct pp_page {
	wchar_t *title;
	wchar_t *tags;
	wchar_t *date;
	wchar_t *content;
	time_t last_modified;
	time_t date_stamp;
	struct pp_page *next;
	struct pp_page *prev;
	wchar_t *icon;
	wchar_t *static_icon;
	bool parsed;
} pp_page; 

struct tag_dict;
typedef struct tag_dict {
	wchar_t *tag;
	struct tag_dict *next;
} tag_dict;

enum sort_types {
	TITLE,
	DATE,
	LAST_MODIFIED
};

pp_page* parse_file(const utf8_path filename);
bool check_dir( const utf8_path p, int mode );
void usage();
void build_new_pragma_site( char *t );
wchar_t *read_file_contents(const utf8_path path);
int write_file_contents(const utf8_path path, const wchar_t *content);
pp_page* load_site(int operation, char* directory);
wchar_t* parse_markdown(wchar_t *markdown);
void append(wchar_t *string, wchar_t *result, size_t *j);
pp_page* merge(pp_page* list1, pp_page* list2);
pp_page* merge_sort(pp_page* head);
void sort_site(pp_page** head);
wchar_t* build_index(pp_page* pages, site_info *site, int start_page);
wchar_t* build_single_page(pp_page* page, site_info *site);
wchar_t* build_scroll(pp_page* pages, site_info *site);
void parse_site_markdown(pp_page* page_list);
char* char_convert(const wchar_t* w);
site_info* load_site_yaml(char* path); 
wchar_t* replace_substring(wchar_t *str, const wchar_t *find, const wchar_t *replace);
void write_single_page(pp_page* page, char* path);
void strip_terminal_newline(wchar_t *s, char *t);
wchar_t* explode_tags(wchar_t* input);
wchar_t* legible_date(time_t when);
wchar_t* string_from_int(long int n);
wchar_t* wrap_with_element(wchar_t* text, wchar_t* start, wchar_t* close);
void load_site_icons(char *root, char *subdir, site_info *config);
void directory_to_array(const utf8_path path, char ***filenames, int *count);
void assign_icons(pp_page *pages, site_info *config);
wchar_t* wchar_convert(const char* c);
pp_page* get_item_by_key(time_t target, pp_page* list);
wchar_t* build_tag_index(pp_page* pages, site_info* site);
void append_tag(wchar_t *tag, tag_dict *tags);
bool tag_list_contains(wchar_t *tag, tag_dict *tags);
bool page_is_tagged(pp_page* p, wchar_t *t);
void swap(tag_dict *a, tag_dict *b);
void sort_tag_list(tag_dict *head);
bool split_before(wchar_t *delim, const wchar_t *input, wchar_t *output);
wchar_t* apply_common_tokens(wchar_t *output, site_info *site, const wchar_t *page_url, const wchar_t *page_title);
void free_page(pp_page *page);
void free_site_info(site_info *config);
void free_page_list(pp_page *head);
