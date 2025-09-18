/**
 * pragma_logger.c - Centralized logging functions for pragma-web
 *
 * Provides consistent logging functionality across the entire codebase.
 * Replaced 
 * replacing the ad hoc printf/wprintf/perror usage with a unified system.
 *
 * Features:
 * - Configurable log levels
 * - Consistent message formatting
 * - Wide character support for existing wprintf usage
 * - Quiet mode support for automation
 * - Proper output routing (stdout for info, stderr for errors)
 *
 * By Will Shaw <wsshaw@gmail.com>
 */

#include "pragma_poison.h"
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <string.h>

// Global logger state
static struct {
    log_level_t min_level;
    bool quiet_mode;
    bool initialized;
} g_logger = {
    .min_level = LOG_INFO,
    .quiet_mode = false,
    .initialized = false
};

// Log level prefixes
static const char* LOG_PREFIXES[] = {
    "",           // LOG_DEBUG (no prefix, just the message)
    "=> ",        // LOG_INFO (informational, not urgent) 
    "! ",         // LOG_WARN (i'm warnin' y'all)
    "! Error: ",  // LOG_ERROR (error, but not a fatal one)
    "! FATAL: "   // LOG_FATAL (error,   ...   a fatal one)
};

// Wide character prefixes for wprintf versions
static const wchar_t* LOG_PREFIXES_W[] = {
    L"",           // LOG_DEBUG
    L"=> ",        // LOG_INFO
    L"! ",         // LOG_WARN
    L"! Error: ",  // LOG_ERROR
    L"! FATAL: "   // LOG_FATAL
};

/**
 * log_init(): Initialize the logging system.
 *
 * arguments:
 *  log_level_t min_level (minimum level to log)
 *  bool quiet_mode (suppress all output if true)
 *
 * returns:
 *  void
 */
void log_init(log_level_t min_level, bool quiet_mode) {
    g_logger.min_level = min_level;
    g_logger.quiet_mode = quiet_mode;
    g_logger.initialized = true;
}

/**
 * should_log(): Check if a message should be logged.
 *
 * arguments:
 *  log_level_t level (level of the message)
 *
 * returns:
 *  bool (true if should log, false otherwise)
 */
static bool should_log(log_level_t level) {
    // Auto-initialize with defaults if not initialized
    if (!g_logger.initialized) {
        log_init(LOG_INFO, false);
    }

    // Don't log if in quiet mode (unless it's an error or fatal)
    if (g_logger.quiet_mode && level < LOG_ERROR) {
        return false;
    }

    // Check minimum level
    return level >= g_logger.min_level;
}

/**
 * get_output_stream(): Get the appropriate output stream for a log level.
 *
 * arguments:
 *  log_level_t level (log level)
 *
 * returns:
 *  FILE* (stdout for info/debug, stderr for warnings/errors)
 */
static FILE* get_output_stream(log_level_t level) {
    return (level >= LOG_WARN) ? stderr : stdout;
}

/**
 * log_debug(): Log a debug message.
 *
 * arguments:
 *  const char *format (printf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_debug(const char *format, ...) {
    if (!should_log(LOG_DEBUG)) return;

    FILE *stream = get_output_stream(LOG_DEBUG);
    va_list args;

    fprintf(stream, "%s", LOG_PREFIXES[LOG_DEBUG]);
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fprintf(stream, "\n");
    fflush(stream);
}

/**
 * log_info(): Log an informational message.
 *
 * arguments:
 *  const char *format (printf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_info(const char *format, ...) {
    if (!should_log(LOG_INFO)) return;

    FILE *stream = get_output_stream(LOG_INFO);
    va_list args;

    fprintf(stream, "%s", LOG_PREFIXES[LOG_INFO]);
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fprintf(stream, "\n");
    fflush(stream);
}

/**
 * log_warn(): Log a warning message.
 *
 * arguments:
 *  const char *format (printf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_warn(const char *format, ...) {
    if (!should_log(LOG_WARN)) return;

    FILE *stream = get_output_stream(LOG_WARN);
    va_list args;

    // for convenience we just use the variadic vprintf() here and in other core logging functions,
    // hence the va_start() stuff 
    fprintf(stream, "%s", LOG_PREFIXES[LOG_WARN]);
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fprintf(stream, "\n");
    fflush(stream);
}

/**
 * log_error(): Log an error message.
 *
 * arguments:
 *  const char *format (printf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_error(const char *format, ...) {
    if (!should_log(LOG_ERROR)) return;

    FILE *stream = get_output_stream(LOG_ERROR);
    va_list args;

    fprintf(stream, "%s", LOG_PREFIXES[LOG_ERROR]);
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fprintf(stream, "\n");
    fflush(stream);
}

/**
 * log_fatal(): Log a fatal error message.
 *
 * arguments:
 *  const char *format (printf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_fatal(const char *format, ...) {
    if (!should_log(LOG_FATAL)) return;

    FILE *stream = get_output_stream(LOG_FATAL);
    va_list args;

    fprintf(stream, "%s", LOG_PREFIXES[LOG_FATAL]);
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fprintf(stream, "\n");
    fflush(stream);
}

/**
 * log_info_w(): Log an informational message with wide characters.
 *
 * arguments:
 *  const wchar_t *format (wprintf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_info_w(const wchar_t *format, ...) {
    if (!should_log(LOG_INFO)) return;

    FILE *stream = get_output_stream(LOG_INFO);
    va_list args;

    fwprintf(stream, L"%ls", LOG_PREFIXES_W[LOG_INFO]);
    va_start(args, format);
    vfwprintf(stream, format, args);
    va_end(args);
    fwprintf(stream, L"\n");
    fflush(stream);
}

/**
 * log_warn_w(): Log a warning message with wide characters.
 *
 * arguments:
 *  const wchar_t *format (wprintf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_warn_w(const wchar_t *format, ...) {
    if (!should_log(LOG_WARN)) return;

    FILE *stream = get_output_stream(LOG_WARN);
    va_list args;

    fwprintf(stream, L"%ls", LOG_PREFIXES_W[LOG_WARN]);
    va_start(args, format);
    vfwprintf(stream, format, args);
    va_end(args);
    fwprintf(stream, L"\n");
    fflush(stream);
}

/**
 * log_error_w(): Log an error message with wide characters.
 *
 * arguments:
 *  const wchar_t *format (wprintf-style format string)
 *  ... (format arguments)
 *
 * returns:
 *  void
 */
void log_error_w(const wchar_t *format, ...) {
    if (!should_log(LOG_ERROR)) return;

    FILE *stream = get_output_stream(LOG_ERROR);
    va_list args;

    fwprintf(stream, L"%ls", LOG_PREFIXES_W[LOG_ERROR]);
    va_start(args, format);
    vfwprintf(stream, format, args);
    va_end(args);
    fwprintf(stream, L"\n");
    fflush(stream);
}

/**
 * log_system_error(): Log a system error with context (replaces perror).
 *
 * arguments:
 *  const char *context (description of what operation failed)
 *
 * returns:
 *  void
 */
void log_system_error(const char *context) {
    if (!should_log(LOG_ERROR)) return;

    FILE *stream = get_output_stream(LOG_ERROR);

    if (context && strlen(context) > 0) {
        fprintf(stream, "%s%s: %s\n", LOG_PREFIXES[LOG_ERROR], context, strerror(errno));
    } else {
        fprintf(stream, "%s%s\n", LOG_PREFIXES[LOG_ERROR], strerror(errno));
    }
    fflush(stream);
}