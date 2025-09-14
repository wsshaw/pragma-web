/**
 * pragma_html_elements.c - HTML element generation functions using safe buffers
 *
 * This module provides a set of functions for generating HTML elements in a safe,
 * consistent manner using the unified buffer management system. These functions
 * handle memory allocation through buffer pooling, proper escaping, and consistent
 * formatting to eliminate the need for manual HTML construction throughout the codebase.
 *
 * All functions now use safe_buffer for efficient memory management and automatic
 * buffer pooling for improved performance.
 *
 * By Will Shaw <wsshaw@gmail.com>
 */

#include "pragma_poison.h"

/**
 * html_escape(): Escape special HTML characters in text.
 *
 * Converts characters that have special meaning in HTML (&, <, >, ", ')
 * to their corresponding HTML entities. This prevents XSS attacks and
 * ensures proper HTML rendering.
 *
 * This function now uses safe_buffer for better performance.
 *
 * arguments:
 *  const wchar_t *text (text to escape; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated escaped text; NULL if input is NULL)
 */
wchar_t* html_escape(const wchar_t *text) {
    if (!text) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    buf->auto_escape = true;  // Enable auto-escape mode

    if (safe_append(text, buf) != 0) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);
    return result;
}

/**
 * html_element(): Create a complete HTML element with optional attributes.
 *
 * Generates a properly formatted HTML element: <tag attributes>content</tag>
 * Content is automatically escaped for safety.
 *
 * arguments:
 *  const wchar_t *tag (element name; must not be NULL)
 *  const wchar_t *content (inner content; may be NULL for empty elements)
 *  const wchar_t *attributes (attribute string like 'class="foo" id="bar"'; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated HTML element; NULL on error)
 */
wchar_t* html_element(const wchar_t *tag, const wchar_t *content, const wchar_t *attributes) {
    if (!tag) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    // Build opening tag
    safe_append(L"<", buf);
    safe_append(tag, buf);

    if (attributes && wcslen(attributes) > 0) {
        safe_append(L" ", buf);
        safe_append(attributes, buf);
    }

    safe_append(L">", buf);

    // Add content (with automatic escaping)
    if (content) {
        buf->auto_escape = true;
        safe_append(content, buf);
        buf->auto_escape = false;
    }

    // Build closing tag
    safe_append(L"</", buf);
    safe_append(tag, buf);
    safe_append(L">", buf);

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);
    return result;
}

/**
 * html_self_closing(): Create a self-closing HTML element.
 *
 * Generates elements like <img>, <br>, <input> that don't have closing tags.
 * Attributes are not escaped (caller should ensure they're safe).
 *
 * arguments:
 *  const wchar_t *tag (element name; must not be NULL)
 *  const wchar_t *attributes (attribute string; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated self-closing element; NULL on error)
 */
wchar_t* html_self_closing(const wchar_t *tag, const wchar_t *attributes) {
    if (!tag) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    safe_append(L"<", buf);
    safe_append(tag, buf);

    if (attributes && wcslen(attributes) > 0) {
        safe_append(L" ", buf);
        safe_append(attributes, buf);
    }

    safe_append(L">", buf);

    wchar_t *result = safe_buffer_to_string(buf);
    buffer_pool_return_global(buf);
    return result;
}

/**
 * html_link(): Create an HTML anchor element.
 *
 * Generates: <a href="url" class="css_class">text</a>
 * URL and text are automatically escaped.
 *
 * arguments:
 *  const wchar_t *href (link URL; must not be NULL)
 *  const wchar_t *text (link text; must not be NULL)
 *  const wchar_t *css_class (CSS class name; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated anchor element; NULL on error)
 */
wchar_t* html_link(const wchar_t *href, const wchar_t *text, const wchar_t *css_class) {
    if (!href || !text) return NULL;

    safe_buffer *buf = buffer_pool_get_global();
    if (!buf) return NULL;

    // Build attributes with escaped href
    safe_buffer *attr_buf = buffer_pool_get_global();
    if (!attr_buf) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    safe_append(L"href=\"", attr_buf);
    attr_buf->auto_escape = true;
    safe_append(href, attr_buf);
    attr_buf->auto_escape = false;
    safe_append(L"\"", attr_buf);

    if (css_class && wcslen(css_class) > 0) {
        safe_append(L" class=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(css_class, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);
    }

    wchar_t *attributes = safe_buffer_to_string(attr_buf);
    buffer_pool_return_global(attr_buf);

    if (!attributes) {
        buffer_pool_return_global(buf);
        return NULL;
    }

    wchar_t *result = html_element(L"a", text, attributes);
    free(attributes);
    buffer_pool_return_global(buf);

    return result;
}

/**
 * html_image(): Create an HTML image element.
 *
 * Generates: <img src="url" alt="alt_text" class="css_class">
 * URL and alt text are automatically escaped.
 *
 * arguments:
 *  const wchar_t *src (image URL; must not be NULL)
 *  const wchar_t *alt (alt text; may be NULL)
 *  const wchar_t *css_class (CSS class name; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated img element; NULL on error)
 */
wchar_t* html_image(const wchar_t *src, const wchar_t *alt, const wchar_t *css_class) {
    if (!src) return NULL;

    safe_buffer *attr_buf = buffer_pool_get_global();
    if (!attr_buf) return NULL;

    safe_append(L"src=\"", attr_buf);
    attr_buf->auto_escape = true;
    safe_append(src, attr_buf);
    attr_buf->auto_escape = false;
    safe_append(L"\"", attr_buf);

    if (alt) {
        safe_append(L" alt=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(alt, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);
    }

    if (css_class && wcslen(css_class) > 0) {
        safe_append(L" class=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(css_class, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);
    }

    wchar_t *attributes = safe_buffer_to_string(attr_buf);
    buffer_pool_return_global(attr_buf);

    if (!attributes) return NULL;

    wchar_t *result = html_self_closing(L"img", attributes);
    free(attributes);

    return result;
}

/**
 * html_div(): Create an HTML div element.
 *
 * Generates: <div class="css_class">content</div>
 * Content is automatically escaped.
 *
 * arguments:
 *  const wchar_t *content (div content; may be NULL for empty div)
 *  const wchar_t *css_class (CSS class name; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated div element; NULL on error)
 */
wchar_t* html_div(const wchar_t *content, const wchar_t *css_class) {
    wchar_t *attributes = NULL;

    if (css_class && wcslen(css_class) > 0) {
        safe_buffer *attr_buf = buffer_pool_get_global();
        if (!attr_buf) return NULL;

        safe_append(L"class=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(css_class, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);

        attributes = safe_buffer_to_string(attr_buf);
        buffer_pool_return_global(attr_buf);

        if (!attributes) return NULL;
    }

    wchar_t *result = html_element(L"div", content, attributes);
    free(attributes);

    return result;
}

/**
 * html_heading(): Create an HTML heading element (h1-h6).
 *
 * Generates: <hN class="css_class">text</hN> where N is the heading level.
 * Text is automatically escaped.
 *
 * arguments:
 *  int level (heading level 1-6; clamped to valid range)
 *  const wchar_t *text (heading text; must not be NULL)
 *  const wchar_t *css_class (CSS class name; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated heading element; NULL on error)
 */
wchar_t* html_heading(int level, const wchar_t *text, const wchar_t *css_class) {
    if (!text) return NULL;

    // Clamp level to valid range
    if (level < 1) level = 1;
    if (level > 6) level = 6;

    wchar_t tag[3];
    swprintf(tag, 3, L"h%d", level);

    wchar_t *attributes = NULL;

    if (css_class && wcslen(css_class) > 0) {
        safe_buffer *attr_buf = buffer_pool_get_global();
        if (!attr_buf) return NULL;

        safe_append(L"class=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(css_class, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);

        attributes = safe_buffer_to_string(attr_buf);
        buffer_pool_return_global(attr_buf);

        if (!attributes) return NULL;
    }

    wchar_t *result = html_element(tag, text, attributes);
    free(attributes);

    return result;
}

/**
 * html_paragraph(): Create an HTML paragraph element.
 *
 * Generates: <p class="css_class">text</p>
 * Text is automatically escaped.
 *
 * arguments:
 *  const wchar_t *text (paragraph text; may be NULL for empty paragraph)
 *  const wchar_t *css_class (CSS class name; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated paragraph element; NULL on error)
 */
wchar_t* html_paragraph(const wchar_t *text, const wchar_t *css_class) {
    wchar_t *attributes = NULL;

    if (css_class && wcslen(css_class) > 0) {
        safe_buffer *attr_buf = buffer_pool_get_global();
        if (!attr_buf) return NULL;

        safe_append(L"class=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(css_class, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);

        attributes = safe_buffer_to_string(attr_buf);
        buffer_pool_return_global(attr_buf);

        if (!attributes) return NULL;
    }

    wchar_t *result = html_element(L"p", text, attributes);
    free(attributes);

    return result;
}

/**
 * html_list_item(): Create an HTML list item element.
 *
 * Generates: <li class="css_class">content</li>
 * Content is automatically escaped.
 *
 * arguments:
 *  const wchar_t *content (list item content; may be NULL for empty item)
 *  const wchar_t *css_class (CSS class name; may be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated list item element; NULL on error)
 */
wchar_t* html_list_item(const wchar_t *content, const wchar_t *css_class) {
    wchar_t *attributes = NULL;

    if (css_class && wcslen(css_class) > 0) {
        safe_buffer *attr_buf = buffer_pool_get_global();
        if (!attr_buf) return NULL;

        safe_append(L"class=\"", attr_buf);
        attr_buf->auto_escape = true;
        safe_append(css_class, attr_buf);
        attr_buf->auto_escape = false;
        safe_append(L"\"", attr_buf);

        attributes = safe_buffer_to_string(attr_buf);
        buffer_pool_return_global(attr_buf);

        if (!attributes) return NULL;
    }

    wchar_t *result = html_element(L"li", content, attributes);
    free(attributes);

    return result;
}