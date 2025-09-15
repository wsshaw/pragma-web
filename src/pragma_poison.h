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
#include <unistd.h>  // for getopt()

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

// Default subdirectories (relative paths, removed hard-coded magic paths :( on 2025-09-14)
#define SITE_SOURCES_DEFAULT_SUBDIR	"dat/"

// Site structure (relative paths within site root)
// Note: SITE_SOURCES and SITE_ROOT are now specified via command line arguments only
#define SITE_POSTS	"c/"			// Directory (within SITE_ROOT) for all generated posts
#define SITE_SCROLL	"s/"			// Directory (within SITE_ROOT) for the scroll, i.e. index of all posts by date
#define SITE_TAG_INDEX	"t/"			// Directory (within SITE_ROOT) for the indices of posts per tag
#define SITE_IMAGES	"img/"			// Directory (within SITE_ROOT) where images are located
#define SITE_ICONS	"img/icons/"		// Directory (within SITE_ROOT) where icons are located
#define SITE_DEFAULT_IMG "img/default.png"	// general fallback image for the site...favico ish
                
#define PRAGMA_DEBUG	0

#define PRAGMA_USAGE	"Usage: pragma -s [source] -o [output]\n\nwhere [source] is the site source directory and [output] is where you want the rendered site.\n\n\t-f: regenerate all html for all nodes\n\t-d: dry run, status report only\n\t-u: regenerate html only for nodes whose source was modified since last successful run\n\t-n: generate html output for new nodes (i.e., created since last run)\n\t-h: show this 'help'\n\nPlease see README.txt for usage details and examples.\n\n"

/**
* Default file contents when creating a new site.  Some of these strings are hideous, but I prefer
* to generate the files from this source rather than template files.  (The pragma executable should
* be able to live anywhere with no supporting files)
*/
#define DEFAULT_YAML	L"---\nsite_name:Web disaster\njs:no\nbuild_tags:yes\nbuild_scroll:yes\ncss:p.css\nheader:_header.html\nfooter:_footer.html\nindex_size:10\nicons_dir:img/icons\ntagline:Comparison is always true due to limited range of data type.\nread_more:-1\ndefault_image:/img/default.png\nbase_url:https://yourdomain.edu/"
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
L"<meta name=\"generator\" content=\"pragma-web\">"\
L"<meta property=\"og:title\" content=\"{TITLE}\">"\
L"<meta property=\"og:description\" content=\"{DESCRIPTION}\">"\
L"<meta property=\"og:type\" content=\"article\">"\
L"<meta property=\"og:locale\" content=\"en\">"\
L"<meta property=\"og:image\" content=\"{MAIN_IMAGE}\">"\
L"<meta property=\"og:site_name\" content=\"{SITE_NAME}\">"\
L"<meta property=\"og:url\" content=\"{PAGE_URL}\">"\
L"<meta name=\"description\" content=\"{DESCRIPTION}\">"\
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
L"<link rel=\"stylesheet\" href=\"/p.css\"></html>"

#define DEFAULT_SAMPLE_POST L"Welcome to pragma!\n"\
L"tags:welcome,sample\n"\
L"date:2024-01-01 12:00:00\n"\
L"icon:default.svg\n"\
L"---\n"\
L"This is your first post! Edit this file in the `dat/` directory to get started.\n\n"\
L"You can add more posts by creating `.txt` files with the same format:\n"\
L"- Title on first line\n"\
L"- Metadata (tags, date, icon) on following lines\n"\
L"- `---` separator\n"\
L"- Content in markdown format\n\n"\
L"#MORE\n\n"\
L"Additional content after the #MORE tag appears only on individual post pages.\n"

#define DEFAULT_ICON_SVG L"<svg width=\"64\" height=\"64\" xmlns=\"http://www.w3.org/2000/svg\">\n"\
L"<rect width=\"64\" height=\"64\" fill=\"#f5f\" rx=\"8\"/>\n"\
L"<text x=\"32\" y=\"40\" font-family=\"Arial\" font-size=\"24\" fill=\"white\" text-anchor=\"middle\">!</text>\n"\
L"</svg>"

// Default template files for new sites (card for use in any context, standalone page, navigation widgets)
#define DEFAULT_TEMPLATE_POST_CARD L"<div class=\"post_card\">\n"\
L"  <div class=\"post_head\">\n"\
L"    <div class=\"post_icon\">\n"\
L"      <img class=\"icon\" alt=\"[icon]\" src=\"/img/icons/{ICON}\">\n"\
L"    </div>\n"\
L"    <div class=\"post_title\">\n"\
L"      <h3><a href=\"{POST_URL}\">{TITLE}</a></h3>\n"\
L"    </div>\n"\
L"    <i>Posted on {DATE}</i><br>\n"\
L"    <!-- IF has_tags -->\n"\
L"    <!-- LOOP tags -->\n"\
L"    <a href=\"{TAG_URL}\">{TAG}</a>\n"\
L"    <!-- END LOOP -->\n"\
L"    <!-- END IF -->\n"\
L"  </div>\n"\
L"  <div class=\"post_in_index\">\n"\
L"    {CONTENT}\n"\
L"  </div>\n"\
L"</div>\n"

#define DEFAULT_TEMPLATE_SINGLE_PAGE L"<div class=\"post_card\">\n"\
L"  <div class=\"post_head\">\n"\
L"    <div class=\"post_icon\">\n"\
L"      <img class=\"icon\" alt=\"[icon]\" src=\"/img/icons/{ICON}\">\n"\
L"    </div>\n"\
L"    <div class=\"post_title\">\n"\
L"      <h3>{TITLE}</h3>\n"\
L"    </div>\n"\
L"    <i>Posted on {DATE}</i><br>\n"\
L"    <!-- IF has_tags -->\n"\
L"    <!-- LOOP tags -->\n"\
L"    <a href=\"{TAG_URL}\">{TAG}</a>\n"\
L"    <!-- END LOOP -->\n"\
L"    <!-- END IF -->\n"\
L"  </div>\n"\
L"  {CONTENT}\n"\
L"</div>\n"

#define DEFAULT_TEMPLATE_NAVIGATION L"<!-- IF has_navigation -->\n"\
L"<!-- IF has_next --><a href=\"{NEXT_URL}\">older</a><!-- END IF -->\n"\
L"<!-- IF has_prev --><!-- IF has_next --> | <!-- END IF --><a href=\"{PREV_URL}\">newer</a><!-- END IF -->\n"\
L"<!-- END IF -->\n"

#define DEFAULT_TEMPLATE_INDEX_ITEM DEFAULT_TEMPLATE_POST_CARD\

#define LOAD_EVERYTHING		0
#define LOAD_METADATA		1
#define LOAD_FILENAMES_ONLY	2
#define LOAD_UPDATED_ONLY	3

#define MAX_MONTHLY_POSTS	128

// TODO: why is this here again?
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
	wchar_t *license;
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
	wchar_t *summary;
	time_t last_modified;
	time_t date_stamp;
	struct pp_page *next;
	struct pp_page *prev;
	wchar_t *icon;
	wchar_t *static_icon;
	wchar_t *source_filename;
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
pp_page* load_site(int operation, char* directory, time_t since_time);
wchar_t* parse_markdown(wchar_t *markdown);
void append(wchar_t *string, wchar_t *result, size_t *j);
pp_page* merge(pp_page* list1, pp_page* list2);
pp_page* merge_sort(pp_page* head);
void sort_site(pp_page** head);
wchar_t* build_index(pp_page* pages, site_info *site, int start_page);
wchar_t* build_single_page(pp_page* page, site_info *site);
wchar_t* build_scroll(pp_page* pages, site_info *site);
wchar_t* build_rss(pp_page* pages, site_info *site);
void parse_site_markdown(pp_page* page_list);
char* char_convert(const wchar_t* w);
site_info* load_site_yaml(char* path); 
wchar_t* replace_substring(wchar_t *str, const wchar_t *find, const wchar_t *replace);
void write_single_page(pp_page* page, char* path, wchar_t* html_content);
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

// HTML element generation functions
wchar_t* html_escape(const wchar_t *text);
wchar_t* html_element(const wchar_t *tag, const wchar_t *content, const wchar_t *attributes);
wchar_t* html_self_closing(const wchar_t *tag, const wchar_t *attributes);
wchar_t* html_link(const wchar_t *href, const wchar_t *text, const wchar_t *css_class);
wchar_t* html_image(const wchar_t *src, const wchar_t *alt, const wchar_t *css_class);
wchar_t* html_image_with_caption(const wchar_t *src, const wchar_t *alt, const wchar_t *caption, const wchar_t *css_class);
wchar_t* html_image_gallery(const wchar_t *directory_path, const wchar_t *css_class);
wchar_t* html_div(const wchar_t *content, const wchar_t *css_class);
wchar_t* html_heading(int level, const wchar_t *text, const wchar_t *css_class);
wchar_t* html_paragraph(const wchar_t *text, const wchar_t *css_class);
wchar_t* html_list_item(const wchar_t *content, const wchar_t *css_class);

// HTML component generation functions
wchar_t* html_post_icon(const wchar_t *icon_filename);
wchar_t* html_post_title(const wchar_t *title_content);
wchar_t* html_post_card_header(const wchar_t *icon_filename, const wchar_t *title_content,
                               const wchar_t *date_content, const wchar_t *tags_content);
wchar_t* html_navigation_links(const wchar_t *prev_href, const wchar_t *next_href,
                              const wchar_t *prev_title, const wchar_t *next_title);
wchar_t* html_post_in_index(const wchar_t *content);
wchar_t* html_read_more_link(const wchar_t *href);
wchar_t* html_complete_post_card(const wchar_t *icon_filename, const wchar_t *title_content,
                                 const wchar_t *date_content, const wchar_t *tags_content,
                                 const wchar_t *post_content, const wchar_t *read_more_href);

// Template system structures and functions
typedef struct {
    wchar_t *title;
    wchar_t *date;
    wchar_t *icon;
    wchar_t *content;
    wchar_t *post_url;
    wchar_t *prev_url;
    wchar_t *next_url;
    wchar_t *prev_title;
    wchar_t *next_title;
    wchar_t *description;

    // Tag arrays
    wchar_t **tags;
    wchar_t **tag_urls;
    int tag_count;

    // Boolean flags
    bool has_prev;
    bool has_next;
    bool has_navigation;
    bool has_next_only;
    bool has_tags;
} template_data;

// Template functions
void template_free(template_data *data);
template_data* template_data_from_page(pp_page *page, site_info *site);
wchar_t* load_template_file(const char *template_path);
wchar_t* template_replace_token(wchar_t *template, const wchar_t *token_name, const wchar_t *replacement_value);
wchar_t* template_process_loop(wchar_t *template, template_data *data);
wchar_t* template_process_conditionals(wchar_t *template, template_data *data);
wchar_t* apply_template(const char *template_path, template_data *data);

// Template helper functions
wchar_t* render_post_card_with_template(pp_page *page, site_info *site);
wchar_t* render_navigation_with_template(pp_page *page, site_info *site);
wchar_t* render_page_with_template(pp_page *page, site_info *site);
wchar_t* render_index_item_with_template(pp_page *page, site_info *site);
wchar_t* strip_html_tags(const wchar_t *input);
wchar_t* get_page_description(pp_page *page);
time_t get_last_run_time(const char *site_directory);
void update_last_run_time(const char *site_directory);
void free_page(pp_page *page);
void free_site_info(site_info *config);
void free_page_list(pp_page *head);

// Enhanced safe buffer system with pooling and automatic escaping
typedef struct {
    wchar_t *buffer;
    size_t size;
    size_t used;
    bool auto_escape;  // Automatically escape HTML when appending 
} safe_buffer;

typedef struct buffer_pool {
    safe_buffer *buffers;
    size_t pool_size;
    size_t next_available;
    bool *in_use;
} buffer_pool;

// Enhanced safe buffer functions
int safe_buffer_init(safe_buffer *buf, size_t initial_size);
int safe_buffer_init_with_escape(safe_buffer *buf, size_t initial_size, bool auto_escape);
int safe_append(const wchar_t *text, safe_buffer *buf);
int safe_append_escaped(const wchar_t *text, safe_buffer *buf);
int safe_append_char(wchar_t c, safe_buffer *buf);
void safe_buffer_reset(safe_buffer *buf);
void safe_buffer_free(safe_buffer *buf);
wchar_t* safe_buffer_to_string(safe_buffer *buf);

// Buffer pool functions
buffer_pool* buffer_pool_create(size_t pool_size, size_t buffer_size);
safe_buffer* buffer_pool_get(buffer_pool *pool);
void buffer_pool_return(buffer_pool *pool, safe_buffer *buf);
void buffer_pool_destroy(buffer_pool *pool);

// Global buffer pool functions (recommended for most HTML generation)
void buffer_pool_init_global(void);
void buffer_pool_cleanup_global(void);
safe_buffer* buffer_pool_get_global(void);
void buffer_pool_return_global(safe_buffer *buf);

// Command-line options structure
typedef struct {
    char *source_dir;
    char *output_dir;
    bool create_site;
    bool force_all;
    bool updated_only;
    bool new_only;
    bool dry_run;
    bool show_help;
} pragma_options;
