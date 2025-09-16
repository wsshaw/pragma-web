/**
 * pragma_html_components.c - Higher-level HTML component generation using safe buffers
 *
 * This module provides functions for generating common HTML components used
 * throughout the pragma site generator. These components combine multiple HTML
 * elements into cohesive structures like post cards, navigation bars, etc.
 *
 * All functions now use safe_buffer for efficient memory management and automatic
 * buffer pooling for improved performance.
 *
 * By Will Shaw <wsshaw@gmail.com>
 */

#include "pragma_poison.h"

/**
 * html_post_icon(): Create a post icon div with image.
 *
 * Generates: <div class="post_icon"><img class="icon" alt="[icon]" src="/img/icons/{icon}"></div>
 * Icon path is automatically escaped.
 *
 * arguments:
 *  const wchar_t *icon_filename (icon filename; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated post icon HTML; NULL on error)
 */
wchar_t* html_post_icon(const wchar_t *icon_filename) {
    if (!icon_filename) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    // Build full icon path: /img/icons/{icon_filename}
    safe_append(L"/img/icons/", buf);
    safe_append(icon_filename, buf);

    wchar_t *icon_path = safe_buffer_to_string(buf);
    if (!icon_path) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    // Create the image element using html_image function
    wchar_t *img = html_image(icon_path, L"[icon]", L"icon");
    free(icon_path);
    buffer_pool_return_global(buf);

    if (!img) return NULL;

    // Wrap in post_icon div
    wchar_t *result = html_div(img, L"post_icon", false);
    free(img);

    return result;
}

/**
 * html_post_title(): Create a post title div.
 *
 * Generates: <div class="post_title">{content}</div>
 * Content is not escaped since it may contain HTML.
 *
 * arguments:
 *  const wchar_t *title_content (title HTML content; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated post title HTML; NULL on error)
 */
wchar_t* html_post_title(const wchar_t *title_content) {
    if (!title_content) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    safe_append(L"<div class=\"post_title\">", buf);
    safe_append(title_content, buf);  // Don't escape - may contain HTML
    safe_append(L"</div>", buf);

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);

    return result;
}

/**
 * html_post_card_header(): Create a complete post card header structure.
 *
 * Generates the standard post header structure:
 * <div class="post_card">
 *   <div class="post_head">
 *     <div class="post_icon"><img class="icon" alt="[icon]" src="/img/icons/{icon}"></div>
 *     <div class="post_title">{title_content}</div>
 *     {date_content}
 *     {tags_content}
 *   </div>
 *
 * arguments:
 *  const wchar_t *icon_filename (icon filename; must not be NULL)
 *  const wchar_t *title_content (title HTML content; must not be NULL)
 *  const wchar_t *date_content (date HTML content; may be NULL)
 *  const wchar_t *tags_content (tags HTML content; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated post card header HTML; NULL on error)
 */
wchar_t* html_post_card_header(const wchar_t *icon_filename, const wchar_t *title_content,
                               const wchar_t *date_content, const wchar_t *tags_content) {
    if (!icon_filename || !title_content) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    // Generate icon div
    wchar_t *icon_html = html_post_icon(icon_filename);
    if (!icon_html) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    // Generate title div
    wchar_t *title_html = html_post_title(title_content);
    if (!title_html) {
        free(icon_html);
        buffer_pool_return_global(buf);
        return NULL;
    }

    // Build the header content
    safe_append(icon_html, buf);
    safe_append(title_html, buf);

    if (date_content) {
        safe_append(date_content, buf);
    }

    if (tags_content) {
        safe_append(tags_content, buf);
    }

    free(icon_html);
    free(title_html);

    wchar_t *head_content = safe_buffer_to_string(buf);
    if (!head_content) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    // Wrap in post_head div, then post_card div
    wchar_t *head_div = html_div(head_content, L"post_head", false);
    free(head_content);
    buffer_pool_return_global(buf);

    if (!head_div) return NULL;

    wchar_t *result = html_div(head_div, L"post_card", false);
    free(head_div);

    return result;
}

/**
 * html_navigation_links(): Create navigation links for prev/next pages.
 *
 * Generates navigation links with proper formatting and post titles.
 * Links are automatically escaped.
 *
 * arguments:
 *  const wchar_t *prev_href (previous page URL; may be NULL)
 *  const wchar_t *next_href (next page URL; may be NULL)
 *  const wchar_t *prev_title (previous page title; may be NULL)
 *  const wchar_t *next_title (next page title; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated navigation HTML; NULL on error)
 */
wchar_t* html_navigation_links(const wchar_t *prev_href, const wchar_t *next_href,
                              const wchar_t *prev_title, const wchar_t *next_title) {
    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    safe_append(L"<nav class=\"post_navigation\">", buf);
    bool has_links = false;

    if (prev_href && prev_title) {
        safe_append(L"<div class=\"nav_prev\">", buf);
        safe_append(L"<span class=\"nav_label\">&laquo; newer</span>", buf);
        wchar_t *prev_link = html_link(prev_href, prev_title, NULL, true);
        if (prev_link) {
            safe_append(L"<div class=\"nav_title\">", buf);
            safe_append(prev_link, buf);
            safe_append(L"</div>", buf);
            free(prev_link);
            has_links = true;
        }
        safe_append(L"</div>", buf);
    }

    if (next_href && next_title) {
        safe_append(L"<div class=\"nav_next\">", buf);
        safe_append(L"<span class=\"nav_label\">older &raquo;</span>", buf);
        wchar_t *next_link = html_link(next_href, next_title, NULL, true);
        if (next_link) {
            safe_append(L"<div class=\"nav_title\">", buf);
            safe_append(next_link, buf);
            safe_append(L"</div>", buf);
            free(next_link);
            has_links = true;
        }
        safe_append(L"</div>", buf);
    }

    safe_append(L"</nav>", buf);

    if (!has_links) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);

    return result;
}

/**
 * html_post_in_index(): Wrap content for display in an index page.
 *
 * Generates: <div class="post_in_index">{content}</div>
 * Content is not escaped since it contains HTML.
 *
 * arguments:
 *  const wchar_t *content (post content; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated wrapped content; NULL on error)
 */
wchar_t* html_post_in_index(const wchar_t *content) {
    if (!content) return NULL;

    return html_div(content, L"post_in_index", false);
}

/**
 * html_read_more_link(): Create a "read more" link.
 *
 * Generates: <p class="read_more"><a href="{href}">read more &raquo;</a></p>
 * URL is automatically escaped but HTML content is preserved.
 *
 * arguments:
 *  const wchar_t *href (link URL; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated read more link; NULL on error)
 */
wchar_t* html_read_more_link(const wchar_t *href) {
    if (!href) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    // Manually create the entire paragraph with link to avoid double-escaping
    safe_append(L"<p class=\"read_more\"><a href=\"", buf);
    buf->auto_escape = true;
    safe_append(href, buf);
    buf->auto_escape = false;
    safe_append(L"\">read more &raquo;</a></p>", buf);

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);

    return result;
}

/**
 * html_complete_post_card(): Create a complete post card with all components.
 *
 * Generates a complete post card structure including header, content, and
 * optional read more link.
 *
 * arguments:
 *  const wchar_t *icon_filename (icon filename; must not be NULL)
 *  const wchar_t *title_content (title HTML content; must not be NULL)
 *  const wchar_t *date_content (date HTML content; may be NULL)
 *  const wchar_t *tags_content (tags HTML content; may be NULL)
 *  const wchar_t *post_content (post body content; must not be NULL)
 *  const wchar_t *read_more_href (read more URL; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated complete post card; NULL on error)
 */
wchar_t* html_complete_post_card(const wchar_t *icon_filename, const wchar_t *title_content,
                                 const wchar_t *date_content, const wchar_t *tags_content,
                                 const wchar_t *post_content, const wchar_t *read_more_href) {
    if (!icon_filename || !title_content || !post_content) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    // Generate header
    wchar_t *header = html_post_card_header(icon_filename, title_content, date_content, tags_content);
    if (!header) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    // Build complete card content
    safe_append(header, buf);
    free(header);

    // Add post content wrapped in post_body div
    wchar_t *body_div = html_div(post_content, L"post_body", false);
    if (!body_div) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    safe_append(body_div, buf);
    free(body_div);

    // Add read more link if provided
    if (read_more_href) {
        wchar_t *read_more = html_read_more_link(read_more_href);
        if (read_more) {
            safe_append(read_more, buf);
            free(read_more);
        }
    }

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);

    return result;
}