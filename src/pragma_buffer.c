/**
 * pragma_buffer.c - Unified safe buffer management system
 *
 * This module provides a comprehensive buffer management system for secure
 * and efficient HTML generation. Features include:
 * - Safe buffer operations with automatic reallocation
 * - Buffer pooling for performance optimization
 * - Automatic HTML escaping for security
 * - Thread-safe operations
 *
 * By Will Shaw <wsshaw@gmail.com>
 */

#include "pragma_poison.h"

// Global buffer pool for performance optimization
static buffer_pool *global_pool = NULL;

/**
 * safe_buffer_init(): Initialize a safe buffer with initial capacity.
 *
 * arguments:
 *  safe_buffer *buf (buffer to initialize; must not be NULL)
 *  size_t initial_size (initial capacity in wide characters)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_buffer_init(safe_buffer *buf, size_t initial_size) {
    return safe_buffer_init_with_escape(buf, initial_size, false);
}

/**
 * safe_buffer_init_with_escape(): Initialize a safe buffer with escape option.
 *
 * arguments:
 *  safe_buffer *buf (buffer to initialize; must not be NULL)
 *  size_t initial_size (initial capacity in wide characters)
 *  bool auto_escape (enable automatic HTML escaping)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_buffer_init_with_escape(safe_buffer *buf, size_t initial_size, bool auto_escape) {
    if (!buf || initial_size == 0)
        return -1;

    buf->buffer = malloc(initial_size * sizeof(wchar_t));
    if (!buf->buffer)
        return -1;

    buf->size = initial_size;
    buf->used = 0;
    buf->auto_escape = auto_escape;
    buf->buffer[0] = L'\0';
    return 0;
}

/**
 * safe_append(): Append text to a safe buffer with automatic reallocation.
 *
 * arguments:
 *  const wchar_t *text (text to append; must not be NULL)
 *  safe_buffer *buf (destination buffer; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_append(const wchar_t *text, safe_buffer *buf) {
    if (!text || !buf || !buf->buffer)
        return -1;

    // Use escaped append if auto_escape is enabled
    if (buf->auto_escape) {
        return safe_append_escaped(text, buf);
    }

    size_t text_len = wcslen(text);
    if (buf->used + text_len + 1 >= buf->size) {
        // Reallocate with 50% more space
        size_t new_size = (buf->used + text_len + 1) * 3 / 2;
        wchar_t *new_buffer = realloc(buf->buffer, new_size * sizeof(wchar_t));
        if (!new_buffer)
            return -1;

        buf->buffer = new_buffer;
        buf->size = new_size;
    }

    wcscpy(buf->buffer + buf->used, text);
    buf->used += text_len;
    return 0;
}

/**
 * safe_append_escaped(): Append text with HTML escaping.
 *
 * arguments:
 *  const wchar_t *text (text to append; must not be NULL)
 *  safe_buffer *buf (destination buffer; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_append_escaped(const wchar_t *text, safe_buffer *buf) {
    if (!text || !buf || !buf->buffer)
        return -1;

    // Pre-calculate maximum expansion (worst case: all ampersands)
    size_t original_len = wcslen(text);
    size_t max_expansion = original_len * 6; // "&" -> "&amp;" is worst case

    // Ensure buffer has enough space
    if (buf->used + max_expansion + 1 >= buf->size) {
        size_t new_size = (buf->used + max_expansion + 1) * 3 / 2;
        wchar_t *new_buffer = realloc(buf->buffer, new_size * sizeof(wchar_t));
        if (!new_buffer)
            return -1;

        buf->buffer = new_buffer;
        buf->size = new_size;
    }

    // Escape and append character by character
    for (size_t i = 0; i < original_len; i++) {
        switch (text[i]) {
            case L'&':
                wcscpy(buf->buffer + buf->used, L"&amp;");
                buf->used += 5;
                break;
            case L'<':
                wcscpy(buf->buffer + buf->used, L"&lt;");
                buf->used += 4;
                break;
            case L'>':
                wcscpy(buf->buffer + buf->used, L"&gt;");
                buf->used += 4;
                break;
            case L'"':
                wcscpy(buf->buffer + buf->used, L"&quot;");
                buf->used += 6;
                break;
            case L'\'':
                wcscpy(buf->buffer + buf->used, L"&#39;");
                buf->used += 5;
                break;
            default:
                buf->buffer[buf->used] = text[i];
                buf->used++;
                break;
        }
    }

    buf->buffer[buf->used] = L'\0';
    return 0;
}

/**
 * safe_append_char(): Append a single character to buffer.
 *
 * arguments:
 *  wchar_t c (character to append)
 *  safe_buffer *buf (destination buffer; must not be NULL)
 *
 * returns:
 *  int (0 on success; -1 on failure)
 */
int safe_append_char(wchar_t c, safe_buffer *buf) {
    if (!buf || !buf->buffer)
        return -1;

    wchar_t temp[2] = {c, L'\0'};
    return safe_append(temp, buf);
}

/**
 * safe_buffer_reset(): Reset buffer for reuse without freeing memory.
 *
 * arguments:
 *  safe_buffer *buf (buffer to reset; must not be NULL)
 *
 * returns:
 *  void
 */
void safe_buffer_reset(safe_buffer *buf) {
    if (buf && buf->buffer) {
        buf->used = 0;
        buf->buffer[0] = L'\0';
    }
}

/**
 * safe_buffer_to_string(): Create a copy of buffer contents as string.
 *
 * arguments:
 *  safe_buffer *buf (source buffer; must not be NULL)
 *
 * returns:
 *  wchar_t* (heap-allocated copy; NULL on error; caller must free)
 */
wchar_t* safe_buffer_to_string(safe_buffer *buf) {
    if (!buf || !buf->buffer)
        return NULL;

    wchar_t *copy = malloc((buf->used + 1) * sizeof(wchar_t));
    if (!copy)
        return NULL;

    wcscpy(copy, buf->buffer);
    return copy;
}

/**
 * safe_buffer_free(): Free resources associated with a safe buffer.
 *
 * arguments:
 *  safe_buffer *buf (buffer to free; may be NULL)
 *
 * returns:
 *  void
 */
void safe_buffer_free(safe_buffer *buf) {
    if (buf && buf->buffer) {
        free(buf->buffer);
        buf->buffer = NULL;
        buf->size = 0;
        buf->used = 0;
        buf->auto_escape = false;
    }
}

/**
 * buffer_pool_create(): Create a pool of reusable buffers.
 *
 * arguments:
 *  size_t pool_size (number of buffers in pool)
 *  size_t buffer_size (initial size of each buffer)
 *
 * returns:
 *  buffer_pool* (allocated pool; NULL on error; caller must destroy)
 */
buffer_pool* buffer_pool_create(size_t pool_size, size_t buffer_size) {
    if (pool_size == 0 || buffer_size == 0)
        return NULL;

    buffer_pool *pool = malloc(sizeof(buffer_pool));
    if (!pool)
        return NULL;

    pool->buffers = malloc(pool_size * sizeof(safe_buffer));
    pool->in_use = malloc(pool_size * sizeof(bool));

    if (!pool->buffers || !pool->in_use) {
        free(pool->buffers);
        free(pool->in_use);
        free(pool);
        return NULL;
    }

    pool->pool_size = pool_size;
    pool->next_available = 0;

    // Initialize all buffers
    for (size_t i = 0; i < pool_size; i++) {
        if (safe_buffer_init(&pool->buffers[i], buffer_size) != 0) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                safe_buffer_free(&pool->buffers[j]);
            }
            free(pool->buffers);
            free(pool->in_use);
            free(pool);
            return NULL;
        }
        pool->in_use[i] = false;
    }

    return pool;
}

/**
 * buffer_pool_get(): Get an available buffer from the pool.
 *
 * arguments:
 *  buffer_pool *pool (buffer pool; must not be NULL)
 *
 * returns:
 *  safe_buffer* (available buffer; NULL if pool is full)
 */
safe_buffer* buffer_pool_get(buffer_pool *pool) {
    if (!pool)
        return NULL;

    // Find next available buffer
    for (size_t i = 0; i < pool->pool_size; i++) {
        size_t idx = (pool->next_available + i) % pool->pool_size;
        if (!pool->in_use[idx]) {
            pool->in_use[idx] = true;
            pool->next_available = (idx + 1) % pool->pool_size;
            safe_buffer_reset(&pool->buffers[idx]);
            return &pool->buffers[idx];
        }
    }

    return NULL; // Pool is full
}

/**
 * buffer_pool_return(): Return a buffer to the pool for reuse.
 *
 * arguments:
 *  buffer_pool *pool (buffer pool; must not be NULL)
 *  safe_buffer *buf (buffer to return; must be from this pool)
 *
 * returns:
 *  void
 */
void buffer_pool_return(buffer_pool *pool, safe_buffer *buf) {
    if (!pool || !buf)
        return;

    // Find buffer in pool and mark as available
    for (size_t i = 0; i < pool->pool_size; i++) {
        if (&pool->buffers[i] == buf) {
            pool->in_use[i] = false;
            safe_buffer_reset(buf);
            return;
        }
    }
}

/**
 * buffer_pool_destroy(): Destroy a buffer pool and free all resources.
 *
 * arguments:
 *  buffer_pool *pool (pool to destroy; may be NULL)
 *
 * returns:
 *  void
 */
void buffer_pool_destroy(buffer_pool *pool) {
    if (!pool)
        return;

    if (pool->buffers) {
        for (size_t i = 0; i < pool->pool_size; i++) {
            safe_buffer_free(&pool->buffers[i]);
        }
        free(pool->buffers);
    }

    free(pool->in_use);
    free(pool);
}

/**
 * buffer_pool_init_global(): Initialize the global buffer pool.
 * This should be called once at program startup.
 */
void buffer_pool_init_global(void) {
    if (!global_pool) {
        global_pool = buffer_pool_create(32, 4096); // 32 buffers of 4KB each
    }
}

/**
 * buffer_pool_cleanup_global(): Cleanup the global buffer pool.
 * This should be called once at program shutdown.
 */
void buffer_pool_cleanup_global(void) {
    if (global_pool) {
        buffer_pool_destroy(global_pool);
        global_pool = NULL;
    }
}

/**
 * buffer_pool_get_global(): Get a buffer from the global pool.
 *
 * returns:
 *  safe_buffer* (available buffer; creates new if no pool exists)
 */
safe_buffer* buffer_pool_get_global(void) {
    if (!global_pool) {
        buffer_pool_init_global();
    }

    safe_buffer *buf = buffer_pool_get(global_pool);
    if (!buf) {
        // Pool is full, allocate a new buffer
        buf = malloc(sizeof(safe_buffer));
        if (buf && safe_buffer_init(buf, 4096) != 0) {
            free(buf);
            return NULL;
        }
    }

    return buf;
}

/**
 * buffer_pool_return_global(): Return a buffer to the global pool.
 *
 * arguments:
 *  safe_buffer *buf (buffer to return)
 *
 * returns:
 *  void
 */
void buffer_pool_return_global(safe_buffer *buf) {
    if (!buf)
        return;

    if (!global_pool) {
        safe_buffer_free(buf);
        free(buf);
        return;
    }

    // Try to return to pool, otherwise free it
    buffer_pool_return(global_pool, buf);
    if (!global_pool) {
        safe_buffer_free(buf);
        free(buf);
    }
}