/**
 * pragma_template_helpers.c - Helper functions for using the template system
 *
 * This module provides convenience functions for common template operations.
 * It demonstrates how to integrate the template system with existing pragma code.
 *
 * By Will Shaw <wsshaw@gmail.com>
 */

#include "pragma_poison.h"

/**
 * render_post_card_with_template(): Render a post card using the template system.
 *
 * This is a demonstration function showing how to use templates instead of
 * manual HTML construction. It replaces the hardcoded HTML building with
 * a template-based approach.
 *
 * arguments:
 *  pp_page *page (page to render; must not be NULL)
 *  site_info *site (site configuration; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated HTML; NULL on error)
 */
wchar_t* render_post_card_with_template(pp_page *page, site_info *site) {
    if (!page || !site) return NULL;

    // Convert page to template data
    template_data *data = template_data_from_page(page, site);
    if (!data) return NULL;

    // Apply template
    wchar_t *result = apply_template("templates/post_card.html", data);

    // Clean up
    template_free(data);

    return result;
}

/**
 * render_navigation_with_template(): Render navigation links using template.
 *
 * Demonstrates template-based navigation rendering instead of manual string building.
 *
 * arguments:
 *  pp_page *page (current page; must not be NULL)
 *  site_info *site (site configuration; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated navigation HTML; NULL on error)
 */
wchar_t* render_navigation_with_template(pp_page *page, site_info *site) {
    if (!page || !site) return NULL;

    template_data *data = template_data_from_page(page, site);
    if (!data) return NULL;

    wchar_t *result = apply_template("templates/navigation.html", data);

    template_free(data);
    return result;
}

/**
 * render_page_with_template(): Render a complete single page using templates.
 *
 * This shows how the page builder could be simplified using the template system.
 * Instead of manual HTML construction and token replacement, we use structured data.
 *
 * arguments:
 *  pp_page *page (page to render; must not be NULL)
 *  site_info *site (site configuration; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated complete page HTML; NULL on error)
 */
wchar_t* render_page_with_template(pp_page *page, site_info *site) {
    if (!page || !site) return NULL;

    // Get template data
    template_data *data = template_data_from_page(page, site);
    if (!data) return NULL;

    // Render the main content using single page template
    wchar_t *page_content = apply_template("templates/single_page.html", data);
    if (!page_content) {
        template_free(data);
        return NULL;
    }

    // Render navigation
    wchar_t *navigation = render_navigation_with_template(page, site);

    // Build complete page (could also be templated)
    size_t total_size = wcslen(site->header) + wcslen(page_content) +
                        (navigation ? wcslen(navigation) : 0) +
                        wcslen(site->footer) + 100;

    wchar_t *complete_page = malloc(total_size * sizeof(wchar_t));
    if (complete_page) {
        wcscpy(complete_page, site->header);
        wcscat(complete_page, page_content);
        if (navigation) {
            wcscat(complete_page, navigation);
        }
        wcscat(complete_page, site->footer);

        // Apply common token replacements
        wchar_t *final_page = apply_common_tokens(complete_page, site, data->post_url, data->title, data->description);
        if (final_page) {
            free(complete_page);
            complete_page = final_page;
        }
    }

    // Clean up
    free(page_content);
    free(navigation);
    template_free(data);

    return complete_page;
}

/**
 * render_index_item_with_template(): Render a post for use in index pages.
 *
 * This function handles the complex index item rendering including content clipping
 * at the "#MORE" delimiter and adding read-more links.
 *
 * arguments:
 *  pp_page *page (page to render; must not be NULL)
 *  site_info *site (site configuration; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated index item HTML; NULL on error)
 */
wchar_t* render_index_item_with_template(pp_page *page, site_info *site) {
    if (!page || !site) return NULL;

    // Create template data
    template_data *data = template_data_from_page(page, site);
    if (!data) return NULL;

    // Handle content clipping at #MORE
    wchar_t *more_delimiter = wcsstr(data->content, L"#MORE");
    bool has_more = (more_delimiter != NULL);

    if (has_more) {
        // Clip content at #MORE and add read-more link
        size_t content_before_more = more_delimiter - data->content;
        wchar_t *clipped_content = malloc((content_before_more + 1) * sizeof(wchar_t));
        if (clipped_content) {
            wcsncpy(clipped_content, data->content, content_before_more);
            clipped_content[content_before_more] = L'\0';

            // Add read-more link
            wchar_t *read_more = html_read_more_link(data->post_url);
            if (read_more) {
                size_t new_size = wcslen(clipped_content) + wcslen(read_more) + 1;
                wchar_t *content_with_link = malloc(new_size * sizeof(wchar_t));
                if (content_with_link) {
                    wcscpy(content_with_link, clipped_content);
                    wcscat(content_with_link, read_more);

                    free(data->content);
                    data->content = content_with_link;
                }
                free(read_more);
            }
            free(clipped_content);
        }
    }

    // Render using template
    wchar_t *result = apply_template("templates/index_item.html", data);

    template_free(data);
    return result;
}