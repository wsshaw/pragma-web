# pragma web — a tiny static site generator

pragma web builds a simple blog/site from Markdown sources. It reads a site config, parses Markdown to HTML, creates index pages, a chronological "scroll," per-tag listings ("tag index"), individual post pages, and a simple RSS feed (feed.xml) of the previous 20 posts.

- **Language:** C 
- **Libraries/Dependencies**: C standard library
- **Status:** Working prototype 
- **License:** MIT


## What it does (at a glance)

- Loads site config (`pragma_config.yml`) and sources from a directory
- Parses Markdown → HTML (headings, paragraphs, inline emphasis, code, basic lists)
- Builds:
  - `index.html`, `index1.html`, … (paged indices)
  - `s/index.html` (date “scroll”)
  - `t/index.html` + `/t/{tag}.html` (tag index + per-tag pages)
  - `/p/{epoch}.html` (one page per post)
- Replaces common tokens with simple templating system, e.g.: `{TITLE}`, `{PAGETITLE}`, `{DATE}`, `{TAGS}`, `{PAGE_URL}`, `{FORWARD}`, `{BACK}`, `{MAIN_IMAGE}`

---

## Quick start
**Building**
The software depends on standard library headers and should build cleanly without special configuration:

` make`
or:
` gcc -O2 -o pragma src/*.c`

After building the executable, the first step is to create a new site, which you can do by passing the -c argument to the executable and specifying the intended path.

`./pragma -c /path/to/site`

You'll need to edit /path/to/site/pragma_config.yml, header/footer templates, etc.

**Generating an Existing Site**

`./pragma -s /path/to/site [-o /path/to/output]`

 If -o is omitted, output defaults to the site source data directory. -h displays basic usage/help.

## Configuration 
- pragma_config.yml (required) lives in the source directory and specifies site metadata, index size, header/footer paths, base URL, icon settings
- Header/footer templates in the site path supply basic layout, pragma generates navigation links and folksonomy catalogues 


## Notes on implementation
- Wide-char parsing is enabled with `setlocale(LC_CTYPE, "en_US.UTF-8");` at the beginning of `main()`, and the argument to `setlocale()` should be changed for other locales (or better yet, I should move it to the configuration file).
- Markdown is handled in pragma_markdown.c; page assembly in pragma_page_builder.c
- Index/scroll/tag/rss builders in corresponding *_builder.c files
- Function headers document basic arguments and usage as well as memory ownership (callers free where noted)

### Known limitations (not critical fixes for this prototype)
- CLI parsing is manual; consider `getopt_long` for richer flags.
- Path building uses `snprintf/swprintf` inconsistently and other methods appear from time to time; needs to be abstracted and made consistent
- Time handling uses localtime(), which technically is not thread-safe but is fine in this case
- Markdown: coverage is basic (no tables, fenced blocks, etc.)
- Memory: obvious leaks fixed; deeper free helpers added (free_page_list()), but not every edge case is audited; tech debt includes a more careful analysis of a lot of the string manipulation functions, which allocate and free often and should have better input sanitization.

A production roadmap would involve systematically addressing these limitations, centralizing error and status logging (with errors to stderr), and better separation of concerns between rendering and site data assembly functions. 