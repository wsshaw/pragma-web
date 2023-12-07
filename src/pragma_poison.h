#include <stdbool.h>

#define MAX_LINE_LENGTH 4096			// ...

// Inputs
#define SITE_SOURCES	"/Users/will/pragma2/dat/"// Input directory - dat files to convert to HTML.  (Default value; specify actual in argv or config)

// Outputs
#define SITE_ROOT	"/Users/will/pp2/"	// Where to stage the generated site (Default value; specify actual in argv or config)
#define SITE_POSTS	"c/"			// Directory (within SITE_ROOT) for all generated posts
#define SITE_SCROLL	"s/"			// Directory (within SITE_ROOT) for the scroll, i.e. index of all posts by date
#define SITE_TAG_INDEX	"t/"			// Directory (within SITE_ROOT) for the indices of posts per tag
#define SITE_IMAGES	"img/"			// Directory (within SITE_ROOT) where images are located
#define SITE_ICONS	"img/icons/"		// Directory (within SITE_ROOT) where icons are located
                
#define PRAGMA_DEBUG	false;

#define PRAGMA_USAGE	"Usage: pragma -s [source] -o [output]\n\nwhere [source] is the site source directory and [output] is where you want the rendered site.\n\n\t-f: regenerate all html for all nodes\n\t-s: dry run, status report only\n\t-u: regenerate html only for nodes whose source was modified since last successful run\n\t-n: generate html output for new nodes (i.e., created since last run)\n\t-h: show this 'help'\n\nPlease see README.txt for usage details and examples.\n\n"

/**
* Default file contents when creating a new site.  Some of these strings are hideous, but I prefer
* to generate the files from this source rather than template files.  (The pragma executable should
* be able to live anywhere with no supporting files)
*/
#define DEFAULT_YAML	L"---\nsite_name:Web disaster\njs:no\nbuild_tags:yes\nbuild_scroll:yes\ncss:p.css\nheader:_header.html\nfooter:_footer.html\nindex_size:10\nbase_url:/\ntagline:Comparison is always true due to limited range of data type.\n"
#define DEFAULT_YAML_FILENAME "pragma_config.yml"
#define DEFAULT_CSS	L"body {\n  margin-left:10em;\n  max-width:50%%;\n}\n\nh2 {\n margin-bottom:2px;\n}\n\ndiv.post_title h3 {\n  margin-bottom:2px;\n  margin-top:0px;\n}\n\n.icon {\n  float:left;\n  width:64px;\n  height:64px;\n  padding-right:10px;\n}\n\nimg.post {\n  display: block;\n  margin-left: auto;\n  margin-right: auto;\n  max-width: 75%%;\n clear:both;\n}\n\ndiv.post_head {\n  display:inline-block;\n  width:100%%;\n  clear:both;\n}\n\ndiv.foot {\nmargin: auto;\nwidth: 15%%;\npadding-top: 1em;\nfont-size: larger;\n}\n\ndiv.post_title {\n vertical-align:top;\n float:left;\n display:inline-block;\n  overflow-wrap: break-word;\n  hyphens: auto;\n width: -webkit-calc(100%% - 80px);\n  width:    -moz-calc(100%% - 80px);\n  width:         calc(100%% - 80px);\n}\n\ndiv.post_image_wrapper {\n  margin-left:auto;\n  margin-right:auto;\n  display:block;\n  clear:both;\n text-align:center;\n}\ndiv.post_body {\n  float:left;\n}\n\n@media only screen and (max-width: 600px) {\n  body{\n  margin-left: 1em;\n  margin-right: 1em;\n  max-width: 100%%;\n  }\n  img.post {\n   display:block;\n        margin-left:auto;\n        margin-right:auto;\n        max-width:100%%;\n        clear:both;\n  }\n}\nblockquote {\n  border:1px solid gray;\n  background-color:#eeeeee;\n  margin-left:5em;\n  margin-right:5em;\n  padding:1em;\n}"
#define DEFAULT_JAVASCRIPT	L""
#define DEFAULT_ABOUT_PAGE	L"[about page here]"

#define DEFAULT_HEADER		L"<!DOCTYPE html>"\
L"<html lang=\"en\">"\
L"<head>"\
L"<meta charset=\"utf-8\">"\
L"<meta name=\"generator\" content=\"pragma_poison_1.0.0\">"\
L"<link rel=\"stylesheet\" href=\"/p.css\">"\
L"<title>#pragma poison | {PAGETITLE}</title>"\
L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"\
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

#define DEFAULT_FOOTER		L"</div></body></html>\n"

#define LOAD_EVERYTHING		0
#define LOAD_METADATA		1
#define LOAD_FILENAMES_ONLY	2

extern const char *pragma_directories[];
extern const char *pragma_basic_files[];

typedef struct site_info {
	wchar_t *site_name;
	bool include_js;
	bool build_tags;
	bool build_scroll;
	wchar_t *css;
	wchar_t *js;
	wchar_t *header;
	wchar_t *footer;
	int index_size;
	wchar_t *tagline;	
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
} pp_page; 

enum sort_types {
	TITLE,
	DATE,
	LAST_MODIFIED
};

pp_page* parse_file(const char* filename);
bool check_dir( const char *p, int mode );
void usage();
void build_new_pragma_site( char *t );
wchar_t *read_file_contents(const char *path);
int write_file_contents(const char *path, const wchar_t *content);
pp_page* load_site(int operation, char* directory);
wchar_t* parse_markdown(wchar_t *markdown);
void append(wchar_t *string, wchar_t *result, size_t *j);
pp_page* merge(pp_page* list1, pp_page* list2);
pp_page* merge_sort(pp_page* head);
void sort_site(pp_page** head);
wchar_t* build_index(pp_page* pages, site_info *site, int start_page);
wchar_t* build_single_page(pp_page* page, site_info *site);
wchar_t* build_scroll(pp_page* pages);
wchar_t* build_tag_index(pp_page* pages);
void parse_site_markdown(pp_page* page_list);
char* char_convert(const wchar_t* w);
site_info* load_site_yaml(char* path); 
wchar_t* replace_substring(wchar_t *str, const wchar_t *find, const wchar_t *replace);
void write_single_page(pp_page* page, char* path);
void strip_terminal_newline(wchar_t *s, char *t);
wchar_t* explode_tags(wchar_t* input);
wchar_t* legible_date(time_t when);
