#include "pragma_poison.h"

/**
 * build_rss(): Generate an RSS 2.0 XML feed from the site pages.
 *
 * Creates a standard RSS feed with channel information from site configuration
 * and items for the most recent posts. Limits output to the 20 most recent posts
 * to keep feed size reasonable.
 *
 * Memory:
 *  Returns a heap-allocated wide-character buffer containing the RSS XML.
 *  Caller is responsible for free().
 *
 * arguments:
 *  pp_page   *pages (head of the linked list of posts; must not be NULL)
 *  site_info *site  (site configuration; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated RSS XML buffer on success; NULL on error)
 *
 * notes:
 *  Uses site->site_name for channel title and site->base_url for links.
 *  Posts should already be sorted by date (most recent first).
 */
wchar_t* build_rss(pp_page* pages, site_info* site) {
    if (!pages || !site) {
        return NULL;
    }

    // Allocate buffer for RSS content (64KB should be plenty for 20 posts)
    size_t buffer_size = 65536;
    wchar_t *rss_output = malloc(buffer_size * sizeof(wchar_t));
    if (!rss_output) {
        perror("Failed to allocate RSS output buffer");
        return NULL;
    }

    // Start with RSS 2.0 header and channel opening
    wcscpy(rss_output, L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    wcscat(rss_output, L"<rss version=\"2.0\">\n");
    wcscat(rss_output, L"<channel>\n");
    
    // Channel metadata
    wcscat(rss_output, L"<title>");
    wcscat(rss_output, site->site_name);
    wcscat(rss_output, L"</title>\n");
    
    wcscat(rss_output, L"<link>");
    wcscat(rss_output, site->base_url);
    wcscat(rss_output, L"</link>\n");
    
    wcscat(rss_output, L"<description>");
    if (site->tagline && wcslen(site->tagline) > 0) {
        wcscat(rss_output, site->tagline);
    } else {
        wcscat(rss_output, L"Latest posts from ");
        wcscat(rss_output, site->site_name);
    }
    wcscat(rss_output, L"</description>\n");
    
    wcscat(rss_output, L"<generator>pragma-web</generator>\n");
    wcscat(rss_output, L"<language>en-us</language>\n");

    // Add items for recent posts (limit to 20)
    int item_count = 0;
    const int max_items = 20;
    
    for (pp_page *current = pages; current != NULL && item_count < max_items; current = current->next) {
        wcscat(rss_output, L"<item>\n");
        
        // Title
        wcscat(rss_output, L"<title>");
        wcscat(rss_output, current->title);
        wcscat(rss_output, L"</title>\n");
        
        // Link
        wcscat(rss_output, L"<link>");
        wcscat(rss_output, site->base_url);
        wcscat(rss_output, L"c/");
        if (current->source_filename) {
            wcscat(rss_output, current->source_filename);
        }
        wcscat(rss_output, L".html</link>\n");

        // GUID (same as link for now)
        wcscat(rss_output, L"<guid>");
        wcscat(rss_output, site->base_url);
        wcscat(rss_output, L"c/");
        if (current->source_filename) {
            wcscat(rss_output, current->source_filename);
        }
        wcscat(rss_output, L".html</guid>\n");
        
        // Publication date (RFC 2822 format)
        wcscat(rss_output, L"<pubDate>");
        struct tm *tm_info = localtime(&current->date_stamp);
        wchar_t *pub_date = malloc(64 * sizeof(wchar_t));
        wcsftime(pub_date, 64, L"%a, %d %b %Y %H:%M:%S %z", tm_info);
        wcscat(rss_output, pub_date);
        free(pub_date);
        wcscat(rss_output, L"</pubDate>\n");
        
        // Description (use summary or first 240 chars of content)
        wcscat(rss_output, L"<description>");
        wchar_t *description = get_page_description(current);
        if (description) {
            wcscat(rss_output, description);
            free(description);
        }
        wcscat(rss_output, L"</description>\n");
        
        wcscat(rss_output, L"</item>\n");
        item_count++;
    }
    
    // Close channel and RSS
    wcscat(rss_output, L"</channel>\n");
    wcscat(rss_output, L"</rss>\n");

    if (PRAGMA_DEBUG) {
        log_info("Generated RSS feed with %d items\n", item_count);
    }

    return rss_output;
}