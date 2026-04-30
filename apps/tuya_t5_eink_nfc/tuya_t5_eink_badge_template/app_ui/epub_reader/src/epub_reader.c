/**
 * @file epub_reader.c
 * @brief EPUB3 Reader Implementation
 *
 * This implementation provides EPUB3 file parsing and reading capabilities.
 * EPUB3 files are ZIP archives containing XML and XHTML files.
 */

#include "epub_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef __linux__
#include <unistd.h> /* for getpid() */
#endif

/* TuyaOS file system support for minizip */
#include "libminizip/ioapi_tkl.h"
#include "tal_log.h"     // TuyaOS log API
#include "tal_memory.h"  // TuyaOS memory API (tal_psram_malloc, tal_free)
#include "tkl_fs.h"      // TuyaOS file system API (for mkdir, etc.)

/* Case-insensitive string comparison helper */
__attribute__((unused)) static int epub_strncasecmp(const char *s1, const char *s2, size_t n)
{
#ifdef __linux__
    /* Use system strncasecmp on Linux */
    return strncasecmp(s1, s2, n);
#else
    /* Fallback implementation */
    for (size_t i = 0; i < n && s1[i] && s2[i]; i++) {
        int c1 = tolower((unsigned char)s1[i]);
        int c2 = tolower((unsigned char)s2[i]);
        if (c1 != c2) {
            return c1 - c2;
        }
    }
    return 0;
#endif
}

#ifdef __linux__
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#endif
#include <stdbool.h>
/* Always use minizip for ZIP extraction */
#include "unzip.h"
#include "ioapi.h"

/* EPUB internal structure */
struct epub_reader_s {
    char                 *filepath;
    char                 *extract_path; /* Temporary extraction directory */
    char                 *opf_path;     /* Path to OPF file */
    epub_metadata_t      *metadata;
    epub_toc_entry_t     *toc;
    epub_content_item_t **spine_items;
    int                   spine_count;
    epub_config_t         config;
    bool                  is_open;

    /* Reading progress */
    int current_chapter;     /* Current chapter index in spine */
    int current_page;        /* Current page within chapter */
    int current_char_offset; /* Current character offset within chapter */
};

/* Default configuration */
epub_config_t epub_get_default_config(void)
{
    epub_config_t config = {
        .chars_per_page = 2000, /* ~2000 characters per page */
        .font_size      = 12,   /* 12pt font */
        .line_height    = 1.5,  /* 1.5x line height */
        .page_width     = 300,  /* 600px width */
        .page_height    = 400,  /* 800px height */
        .font_path      = NULL  /* Will be set to LVGL default if NULL */
    };
    return config;
}

/* Helper: Read file contents */
static char *read_file_contents(const char *filepath, size_t *size)
{
#ifdef __linux__
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        PR_ERR("[EPUB] Failed to open file (fopen): %s", filepath);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t read  = fread(buffer, 1, file_size, fp);
    buffer[read] = '\0';
    fclose(fp);

    if (size) {
        *size = read;
    }

    return buffer;
#else
    /* For MCU: Use TuyaOS file system API */
    TUYA_FILE fp = tkl_fopen(filepath, "rb");
    if (!fp) {
        PR_ERR("[EPUB] Failed to open file (tkl_fopen): %s", filepath);
        return NULL;
    }

    tkl_fseek(fp, 0, SEEK_END);
    INT64_T file_size = tkl_ftell(fp);
    tkl_fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        PR_ERR("[EPUB] Invalid file size: %lld for %s", (long long)file_size, filepath);
        tkl_fclose(fp);
        return NULL;
    }

    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        PR_ERR("[EPUB] Failed to allocate buffer for file: %s", filepath);
        tkl_fclose(fp);
        return NULL;
    }

    INT_T read_bytes = tkl_fread(buffer, (INT_T)file_size, fp);
    tkl_fclose(fp);
    
    if (read_bytes <= 0) {
        PR_ERR("[EPUB] Failed to read file: %s, read_bytes=%d", filepath, read_bytes);
        free(buffer);
        return NULL;
    }

    buffer[read_bytes] = '\0';

    if (size) {
        *size = (size_t)read_bytes;
    }
    
    PR_DEBUG("[EPUB] Successfully read file: %s, size=%d", filepath, read_bytes);

    return buffer;
#endif
}

/* Helper: Check if file exists */
static bool file_exists(const char *filepath)
{
#ifdef __linux__
    FILE *fp = fopen(filepath, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
#else
    /* For MCU: Use TuyaOS file system API */
    BOOL_T is_exist = FALSE;
    if (tkl_fs_is_exist(filepath, &is_exist) == 0 && is_exist) {
        return true;
    }
    
    /* Fallback: try to open the file */
    TUYA_FILE fp = tkl_fopen(filepath, "r");
    if (fp) {
        tkl_fclose(fp);
        return true;
    }
    return false;
#endif
}

/* Helper: Check if path is a known mount point */
#ifndef __linux__
static bool is_mount_point(const char *path)
{
    /* Known mount points that should not be created with mkdir */
    const char *mount_points[] = {
        "/sdcard",
        "/mnt",
        "/tmp",
        NULL
    };
    
    for (int i = 0; mount_points[i] != NULL; i++) {
        if (strcmp(path, mount_points[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Helper: Create directory recursively (MCU implementation) */
static bool create_directory_recursive(const char *path)
{
    BOOL_T is_exist = FALSE;
    
    /* Check if directory already exists */
    if (tkl_fs_is_exist(path, &is_exist) == 0 && is_exist) {
        return true; /* Already exists */
    }
    
    /* Create a copy of the path for modification */
    char *path_copy = strdup(path);
    if (!path_copy) {
        PR_ERR("[EPUB] Failed to allocate memory for path copy");
        return false;
    }
    
    /* Create parent directories first */
    char *p = path_copy;
    
    /* Skip leading slash */
    if (*p == '/') {
        p++;
    }
    
    /* Iterate through path and create each directory level */
    while (*p) {
        /* Find next slash */
        while (*p && *p != '/') {
            p++;
        }
        
        if (*p == '/') {
            *p = '\0'; /* Temporarily terminate string */
            
            /* Skip mount points - they should already exist */
            if (is_mount_point(path_copy)) {
                PR_DEBUG("[EPUB] Skipping mount point: %s", path_copy);
                *p = '/'; /* Restore slash */
                p++;
                continue;
            }
            
            /* Check if this level exists */
            is_exist = FALSE;
            if (tkl_fs_is_exist(path_copy, &is_exist) != 0 || !is_exist) {
                /* Create this directory level */
                INT_T ret = tkl_fs_mkdir(path_copy);
                if (ret != 0) {
                    /* Try to verify if it actually exists (maybe created by another thread) */
                    is_exist = FALSE;
                    if (tkl_fs_is_exist(path_copy, &is_exist) != 0 || !is_exist) {
                        PR_ERR("[EPUB] Failed to create directory: %s, ret=%d", path_copy, ret);
                        free(path_copy);
                        return false;
                    }
                    /* Directory exists now, continue */
                    PR_DEBUG("[EPUB] Directory exists after retry: %s", path_copy);
                } else {
                    PR_DEBUG("[EPUB] Created directory: %s", path_copy);
                }
            } else {
                PR_DEBUG("[EPUB] Directory already exists: %s", path_copy);
            }
            
            *p = '/'; /* Restore slash */
            p++;
        }
    }
    
    /* Skip mount points for final level too */
    if (is_mount_point(path_copy)) {
        PR_DEBUG("[EPUB] Skipping mount point (final): %s", path_copy);
        free(path_copy);
        return true;
    }
    
    /* Create final directory level */
    is_exist = FALSE;
    if (tkl_fs_is_exist(path_copy, &is_exist) != 0 || !is_exist) {
        INT_T ret = tkl_fs_mkdir(path_copy);
        if (ret != 0) {
            /* Check again if it exists (race condition) */
            is_exist = FALSE;
            if (tkl_fs_is_exist(path_copy, &is_exist) != 0 || !is_exist) {
                PR_ERR("[EPUB] Failed to create final directory: %s, ret=%d", path_copy, ret);
                free(path_copy);
                return false;
            }
            PR_DEBUG("[EPUB] Final directory exists after retry: %s", path_copy);
        } else {
            PR_DEBUG("[EPUB] Created final directory: %s", path_copy);
        }
    } else {
        PR_DEBUG("[EPUB] Final directory already exists: %s", path_copy);
    }
    
    free(path_copy);
    PR_DEBUG("[EPUB] Directory created successfully: %s", path);
    return true;
}
#endif

/* Helper: Create directory (platform-specific) */
static bool create_directory(const char *path)
{
#ifdef __linux__
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    return (system(cmd) == 0);
#else
    /* For MCU: Use custom recursive implementation */
    return create_directory_recursive(path);
#endif
}

/* Helper: Remove directory (platform-specific) */
static bool remove_directory(const char *path)
{
#ifdef __linux__
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    return (system(cmd) == 0);
#else
    /* For MCU: Replace with your file system API */
    /* Example: return fs_rmdir_recursive(path) == 0; */
    /* For now, just return true - cleanup handled by file system */
    (void)path;
    return true;
#endif
}

/* Helper: Get cache directory path */
static char *get_cache_dir(void)
{
    static char cache_dir[512];
    /* Use SD card for cache */
    snprintf(cache_dir, sizeof(cache_dir), "/sdcard/.cache/epub_reader");
    return cache_dir;
}

/* Helper: Create cache directory if it doesn't exist */
static bool ensure_cache_dir(void)
{
    char *cache_dir = get_cache_dir();

    /* Check if directory already exists */
    struct stat st;
    if (stat(cache_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* Directory exists, verify it's writable */
#ifdef __linux__
        return (access(cache_dir, W_OK) == 0);
#else
        return true;
#endif
    }

    /* Directory doesn't exist, create it */
    if (!create_directory(cache_dir)) {
        return false;
    }

    /* Verify directory was created */
    return (stat(cache_dir, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Helper: Generate cache filename from EPUB file path */
static char *get_cache_filename(const char *filepath)
{
    static char cache_file[512];
    char       *cache_dir = get_cache_dir();

    /* Extract filename from path */
    const char *filename = strrchr(filepath, '/');
    if (!filename) {
        filename = strrchr(filepath, '\\');
    }
    if (!filename) {
        filename = filepath;
    } else {
        filename++; /* Skip the slash */
    }

    /* Sanitize filename: replace invalid characters with underscore */
    char   sanitized[256];
    size_t j = 0;
    for (int i = 0; filename[i] && j < sizeof(sanitized) - 1; i++) {
        char c = filename[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
            c == '_') {
            sanitized[j++] = c;
        } else {
            sanitized[j++] = '_';
        }
    }
    sanitized[j] = '\0';

    snprintf(cache_file, sizeof(cache_file), "%s/%s.cache", cache_dir, sanitized);
    return cache_file;
}

/* Helper: Escape string for cache file (replace newlines and special chars) */
static void escape_string_for_cache(FILE *f, const char *str)
{
    if (!str) {
        fprintf(f, "\\0");
        return;
    }
    for (const char *p = str; *p; p++) {
        if (*p == '\n') {
            fprintf(f, "\\n");
        } else if (*p == '\r') {
            fprintf(f, "\\r");
        } else if (*p == '\t') {
            fprintf(f, "\\t");
        } else if (*p == '\\') {
            fprintf(f, "\\\\");
        } else if (*p == '|') {
            fprintf(f, "\\|");
        } else {
            fputc(*p, f);
        }
    }
}

/* Helper: Unescape string from cache file */
static char *unescape_string_from_cache(const char *escaped)
{
    if (!escaped || strcmp(escaped, "\\0") == 0) {
        return NULL;
    }

    size_t len    = strlen(escaped);
    char  *result = (char *)malloc(len + 1);
    if (!result) {
        return NULL;
    }

    int j = 0;
    for (int i = 0; escaped[i]; i++) {
        if (escaped[i] == '\\' && escaped[i + 1]) {
            i++;
            switch (escaped[i]) {
            case 'n':
                result[j++] = '\n';
                break;
            case 'r':
                result[j++] = '\r';
                break;
            case 't':
                result[j++] = '\t';
                break;
            case '\\':
                result[j++] = '\\';
                break;
            case '|':
                result[j++] = '|';
                break;
            case '0':
                return NULL; /* NULL string marker */
            default:
                result[j++] = escaped[i];
                break;
            }
        } else {
            result[j++] = escaped[i];
        }
    }
    result[j] = '\0';
    return result;
}

/* Save cache to file */
static bool save_cache(epub_reader_t *reader)
{
    if (!reader || !reader->filepath) {
        return false;
    }

    if (!ensure_cache_dir()) {
        return false;
    }

    char *cache_file = get_cache_filename(reader->filepath);
    FILE *f          = fopen(cache_file, "w");
    if (!f) {
        return false;
    }

    /* Cache file is being written - directory should exist now */
    printf("Cache saved to: %s\n", cache_file);

    /* Write version marker */
    fprintf(f, "EPUB_CACHE_V1\n");

    /* Write OPF path */
    fprintf(f, "OPF:");
    if (reader->opf_path) {
        escape_string_for_cache(f, reader->opf_path);
    }
    fprintf(f, "\n");

    /* Write metadata */
    if (reader->metadata) {
        fprintf(f, "META:");
        escape_string_for_cache(f, reader->metadata->title);
        fprintf(f, "|");
        escape_string_for_cache(f, reader->metadata->author);
        fprintf(f, "|");
        escape_string_for_cache(f, reader->metadata->language);
        fprintf(f, "|");
        escape_string_for_cache(f, reader->metadata->publisher);
        fprintf(f, "|");
        escape_string_for_cache(f, reader->metadata->date);
        fprintf(f, "|");
        escape_string_for_cache(f, reader->metadata->identifier);
        fprintf(f, "\n");
    }

    /* Write spine items */
    fprintf(f, "SPINE:%d\n", reader->spine_count);
    for (int i = 0; i < reader->spine_count; i++) {
        if (reader->spine_items[i]) {
            fprintf(f, "ITEM:");
            escape_string_for_cache(f, reader->spine_items[i]->id);
            fprintf(f, "|");
            escape_string_for_cache(f, reader->spine_items[i]->href);
            fprintf(f, "|");
            escape_string_for_cache(f, reader->spine_items[i]->media_type);
            fprintf(f, "\n");
        }
    }

    /* Write TOC */
    if (reader->toc) {
        fprintf(f, "TOC_START\n");
        epub_toc_entry_t *entry = reader->toc;
        while (entry) {
            fprintf(f, "TOC_ENTRY:%d:", entry->level);
            escape_string_for_cache(f, entry->label);
            fprintf(f, "|");
            escape_string_for_cache(f, entry->href);
            fprintf(f, "\n");
            if (entry->children) {
                fprintf(f, "TOC_CHILD_START\n");
                epub_toc_entry_t *child = entry->children;
                while (child) {
                    fprintf(f, "TOC_ENTRY:%d:", child->level);
                    escape_string_for_cache(f, child->label);
                    fprintf(f, "|");
                    escape_string_for_cache(f, child->href);
                    fprintf(f, "\n");
                    child = child->next;
                }
                fprintf(f, "TOC_CHILD_END\n");
            }
            entry = entry->next;
        }
        fprintf(f, "TOC_END\n");
    }

    /* Write reading progress */
    fprintf(f, "PROGRESS:%d:%d:%d\n", reader->current_chapter, reader->current_page, reader->current_char_offset);

    /* Write font configuration (legacy EPUB config) */
    fprintf(f, "CONFIG:");
    fprintf(f, "chars_per_page=%d|", reader->config.chars_per_page);
    fprintf(f, "font_size=%d|", reader->config.font_size);
    fprintf(f, "line_height=%d|", (int)(reader->config.line_height * 100)); /* Store as integer (multiply by 100) */
    fprintf(f, "page_width=%d|", reader->config.page_width);
    fprintf(f, "page_height=%d|", reader->config.page_height);
    fprintf(f, "font_path=");
    if (reader->config.font_path) {
        escape_string_for_cache(f, reader->config.font_path);
    } else {
        /* Default to LVGL default font */
        escape_string_for_cache(f, "lv_font_montserrat_14");
    }
    fprintf(f, "\n");

    fclose(f);
    return true;
}

/* Save LVGL display configuration to cache file */
bool epub_save_lvgl_config(epub_reader_t *reader, const char *font_path, int line_spacing, int letter_spacing,
                            int margin_left, int margin_right, int margin_top, int margin_bottom,
                            int text_area_width, int text_area_height, int line_height, int line_spacing_px)
{
    if (!reader || !reader->filepath) {
        return false;
    }

    char *cache_file = get_cache_filename(reader->filepath);

    /* Read entire file, remove existing LVGL_CONFIG if present, append new one */
    FILE *f = fopen(cache_file, "r");
    if (!f) {
        return false;
    }

    /* Read all lines into memory */
    char **lines = NULL;
    int line_count = 0;
    int lvgl_config_line = -1;
    char line[2048];

    while (fgets(line, sizeof(line), f)) {
        /* Check if this is LVGL_CONFIG line */
        if (strncmp(line, "LVGL_CONFIG:", 12) == 0) {
            lvgl_config_line = line_count;
        }
        lines = (char **)realloc(lines, (line_count + 1) * sizeof(char *));
        lines[line_count] = (char *)malloc(strlen(line) + 1);
        strcpy(lines[line_count], line);
        line_count++;
    }
    fclose(f);

    /* Remove LVGL_CONFIG line if it exists */
    if (lvgl_config_line >= 0) {
        free(lines[lvgl_config_line]);
        for (int i = lvgl_config_line; i < line_count - 1; i++) {
            lines[i] = lines[i + 1];
        }
        line_count--;
    }

    /* Write file back with new LVGL_CONFIG appended */
    f = fopen(cache_file, "w");
    if (!f) {
        /* Free memory */
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        return false;
    }

    /* Write all lines */
    for (int i = 0; i < line_count; i++) {
        fputs(lines[i], f);
        free(lines[i]);
    }
    free(lines);

    /* Write LVGL display configuration */
    fprintf(f, "LVGL_CONFIG:");
    fprintf(f, "font_path=");
    if (font_path) {
        escape_string_for_cache(f, font_path);
    } else {
        escape_string_for_cache(f, "");
    }
    fprintf(f, "|line_spacing=%d|", line_spacing);
    fprintf(f, "letter_spacing=%d|", letter_spacing);
    fprintf(f, "margin_left=%d|", margin_left);
    fprintf(f, "margin_right=%d|", margin_right);
    fprintf(f, "margin_top=%d|", margin_top);
    fprintf(f, "margin_bottom=%d|", margin_bottom);
    fprintf(f, "text_area_width=%d|", text_area_width);
    fprintf(f, "text_area_height=%d|", text_area_height);
    fprintf(f, "line_height=%d|", line_height);
    fprintf(f, "line_spacing_px=%d", line_spacing_px);
    fprintf(f, "\n");

    fclose(f);
    return true;
}

/* Load cache from file */
static bool load_cache(epub_reader_t *reader)
{
    if (!reader || !reader->filepath) {
        return false;
    }

    char *cache_file = get_cache_filename(reader->filepath);
    if (!file_exists(cache_file)) {
        return false;
    }

    FILE *f = fopen(cache_file, "r");
    if (!f) {
        return false;
    }

    char line[2048];

    /* Read version */
    if (!fgets(line, sizeof(line), f) || strncmp(line, "EPUB_CACHE_V1", 13) != 0) {
        fclose(f);
        return false;
    }

    /* Read OPF path */
    if (fgets(line, sizeof(line), f) && strncmp(line, "OPF:", 4) == 0) {
        char *opf = unescape_string_from_cache(line + 4);
        if (opf && strlen(opf) > 0) {
            reader->opf_path = opf;
        } else {
            free(opf);
        }
    }

    /* Read metadata */
    if (fgets(line, sizeof(line), f) && strncmp(line, "META:", 5) == 0) {
        reader->metadata = (epub_metadata_t *)calloc(1, sizeof(epub_metadata_t));
        if (reader->metadata) {
            char *p = line + 5;
            char *fields[6];
            int   field_idx = 0;
            char *start     = p;

            for (; *p && field_idx < 6; p++) {
                if (*p == '|') {
                    *p                  = '\0';
                    fields[field_idx++] = start;
                    start               = p + 1;
                }
            }
            if (field_idx < 6) {
                fields[field_idx++] = start;
            }

            if (field_idx >= 1)
                reader->metadata->title = unescape_string_from_cache(fields[0]);
            if (field_idx >= 2)
                reader->metadata->author = unescape_string_from_cache(fields[1]);
            if (field_idx >= 3)
                reader->metadata->language = unescape_string_from_cache(fields[2]);
            if (field_idx >= 4)
                reader->metadata->publisher = unescape_string_from_cache(fields[3]);
            if (field_idx >= 5)
                reader->metadata->date = unescape_string_from_cache(fields[4]);
            if (field_idx >= 6)
                reader->metadata->identifier = unescape_string_from_cache(fields[5]);
        }
    }

    /* Read spine items */
    if (fgets(line, sizeof(line), f) && strncmp(line, "SPINE:", 6) == 0) {
        reader->spine_count = atoi(line + 6);
        if (reader->spine_count > 0) {
            reader->spine_items = (epub_content_item_t **)calloc(reader->spine_count, sizeof(epub_content_item_t *));
            for (int i = 0; i < reader->spine_count; i++) {
                if (fgets(line, sizeof(line), f) && strncmp(line, "ITEM:", 5) == 0) {
                    reader->spine_items[i] = (epub_content_item_t *)calloc(1, sizeof(epub_content_item_t));
                    if (reader->spine_items[i]) {
                        char *p = line + 5;
                        char *fields[3];
                        int   field_idx = 0;
                        char *start     = p;

                        for (; *p && field_idx < 3; p++) {
                            if (*p == '|') {
                                *p                  = '\0';
                                fields[field_idx++] = start;
                                start               = p + 1;
                            }
                        }
                        if (field_idx < 3) {
                            fields[field_idx++] = start;
                        }

                        if (field_idx >= 1)
                            reader->spine_items[i]->id = unescape_string_from_cache(fields[0]);
                        if (field_idx >= 2)
                            reader->spine_items[i]->href = unescape_string_from_cache(fields[1]);
                        if (field_idx >= 3)
                            reader->spine_items[i]->media_type = unescape_string_from_cache(fields[2]);
                    }
                }
            }
        }
    }

    /* Read TOC - simplified, just read first level for now */
    epub_toc_entry_t *last_toc = NULL;
    bool              toc_done = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TOC_ENTRY:", 10) == 0) {
            if (toc_done)
                continue; /* Skip TOC entries after TOC_END */
            epub_toc_entry_t *entry = (epub_toc_entry_t *)calloc(1, sizeof(epub_toc_entry_t));
            if (entry) {
                char *p      = line + 10;
                entry->level = atoi(p);
                while (*p && *p != ':')
                    p++;
                if (*p == ':')
                    p++;

                char *fields[2];
                int   field_idx = 0;
                char *start     = p;

                for (; *p && field_idx < 2; p++) {
                    if (*p == '|') {
                        *p                  = '\0';
                        fields[field_idx++] = start;
                        start               = p + 1;
                    }
                }
                if (field_idx < 2) {
                    fields[field_idx++] = start;
                }

                if (field_idx >= 1)
                    entry->label = unescape_string_from_cache(fields[0]);
                if (field_idx >= 2)
                    entry->href = unescape_string_from_cache(fields[1]);

                if (!reader->toc) {
                    reader->toc = entry;
                } else if (last_toc) {
                    last_toc->next = entry;
                }
                last_toc = entry;
            }
        } else if (strncmp(line, "TOC_END", 7) == 0) {
            toc_done = true; /* Mark TOC as done, but continue reading for PROGRESS */
        } else if (strncmp(line, "PROGRESS:", 9) == 0) {
            /* Read reading progress */
            char *p                 = line + 9;
            reader->current_chapter = atoi(p);
            while (*p && *p != ':')
                p++;
            if (*p == ':') {
                p++;
                reader->current_page = atoi(p);
            }
            while (*p && *p != ':')
                p++;
            if (*p == ':') {
                p++;
                reader->current_char_offset = atoi(p);
            }
            printf("Cache: Loaded PROGRESS line: Chapter=%d, Page=%d, Offset=%d\n", reader->current_chapter,
                   reader->current_page, reader->current_char_offset);
            fflush(stdout);
        } else if (strncmp(line, "CONFIG:", 7) == 0) {
            /* Read configuration */
            char *p           = line + 7;
            char *key_start   = p;
            char *value_start = NULL;

            while (*p) {
                if (*p == '=') {
                    *p          = '\0';
                    value_start = p + 1;
                } else if (*p == '|') {
                    *p = '\0';
                    if (key_start && value_start) {
                        if (strcmp(key_start, "chars_per_page") == 0) {
                            reader->config.chars_per_page = atoi(value_start);
                        } else if (strcmp(key_start, "font_size") == 0) {
                            reader->config.font_size = atoi(value_start);
                        } else if (strcmp(key_start, "line_height") == 0) {
                            reader->config.line_height = atoi(value_start) / 100.0f;
                        } else if (strcmp(key_start, "page_width") == 0) {
                            reader->config.page_width = atoi(value_start);
                        } else if (strcmp(key_start, "page_height") == 0) {
                            reader->config.page_height = atoi(value_start);
                        } else if (strcmp(key_start, "font_path") == 0) {
                            /* Free existing font_path if any */
                            if (reader->config.font_path) {
                                free(reader->config.font_path);
                            }
                            reader->config.font_path = unescape_string_from_cache(value_start);
                        }
                    }
                    key_start   = p + 1;
                    value_start = NULL;
                }
                p++;
            }
            /* Handle last field */
            if (key_start && value_start) {
                if (strcmp(key_start, "font_path") == 0) {
                    if (reader->config.font_path) {
                        free(reader->config.font_path);
                    }
                    reader->config.font_path = unescape_string_from_cache(value_start);
                }
            }
            /* If font_path is NULL or empty, set to LVGL default */
            if (!reader->config.font_path || strlen(reader->config.font_path) == 0) {
                if (reader->config.font_path) {
                    free(reader->config.font_path);
                }
                reader->config.font_path = strdup("lv_font_montserrat_14");
            }
        }
    }

    fclose(f);
    return true;
}

/* Helper: Extract ZIP file to directory using minizip 
 * 
 * ⚠️ WARNING: This is a TIME-CONSUMING operation (may take several seconds)!
 * - Should NOT be called from LVGL event handlers directly
 * - Caller must release LVGL lock (lv_unlock) before calling
 * - Or execute in a separate thread
 * - Progress: Extracting EPUB can take 1-5 seconds depending on file size
 */
static bool extract_zip(const char *zip_path, const char *extract_dir)
{
    /* Initialize TuyaOS file system interface for minizip */
    zlib_filefunc_def filefunc;
    fill_tkl_filefunc(&filefunc);
    
    /* Open ZIP file using minizip with TuyaOS file system */
    unzFile zip = unzOpen2(zip_path, &filefunc);
    if (!zip) {
        return false;
    }

    /* Create extraction directory */
    if (!create_directory(extract_dir)) {
        unzClose(zip);
        return false;
    }

    /* Get global info */
    unz_global_info global_info;
    if (unzGetGlobalInfo(zip, &global_info) != UNZ_OK) {
        unzClose(zip);
        return false;
    }

    /* Extract all files */
    PR_DEBUG("[EPUB_ZIP] Starting file extraction, total entries=%d", global_info.number_entry);
    int extracted_count = 0;
    int dir_count = 0;
    int file_count = 0;
    
    int first_file_ret = unzGoToFirstFile(zip);
    PR_DEBUG("[EPUB_ZIP] unzGoToFirstFile returned: %d (UNZ_OK=%d)", first_file_ret, UNZ_OK);
    
    if (first_file_ret == UNZ_OK) {
        do {
            char          filename_inzip[512];
            unz_file_info file_info;

            /* Get file info */
            if (unzGetCurrentFileInfo(zip, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) !=
                UNZ_OK) {
                PR_ERR("[EPUB_ZIP] Failed to get file info");
                continue;
            }
            
            PR_DEBUG("[EPUB_ZIP] Found entry: %s (size=%lu)", filename_inzip, (unsigned long)file_info.uncompressed_size);

            /* Skip directories */
            if (filename_inzip[strlen(filename_inzip) - 1] == '/') {
                PR_DEBUG("[EPUB_ZIP] Skipping directory: %s", filename_inzip);
                dir_count++;
                continue;
            }
            
            file_count++;

            /* Build full path */
            char full_path[512];  /* Reduced for embedded systems: 256 base + 256 for internal paths */
            int path_len = snprintf(full_path, sizeof(full_path), "%s/%s", extract_dir, filename_inzip);
            if (path_len >= (int)sizeof(full_path)) {
                PR_ERR("[EPUB_ZIP] Path too long (truncated): %s", full_path);
                continue;  /* Skip this file to avoid corruption */
            }
            PR_DEBUG("[EPUB_ZIP] Extracting file #%d: %s -> %s", file_count, filename_inzip, full_path);

            /* Create directory if needed */
            char *dir_path   = strdup(full_path);
            if (!dir_path) {
                PR_ERR("[EPUB_ZIP] Failed to allocate memory for dir_path");
                continue;
            }
            char *last_slash = strrchr(dir_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                PR_DEBUG("[EPUB_ZIP] Creating parent directory: %s", dir_path);
                if (!create_directory(dir_path)) {
                    PR_ERR("[EPUB_ZIP] Failed to create parent directory: %s", dir_path);
                    free(dir_path);
                    continue;
                }
                free(dir_path);
            } else {
                free(dir_path);
            }

            /* Open file in zip */
            PR_DEBUG("[EPUB_ZIP] Opening file in ZIP: %s", filename_inzip);
            int open_ret = unzOpenCurrentFile(zip);
            PR_DEBUG("[EPUB_ZIP] unzOpenCurrentFile returned: %d (UNZ_OK=%d)", open_ret, UNZ_OK);
            
            if (open_ret == UNZ_OK) {
#ifdef __linux__
                FILE *out = fopen(full_path, "wb");
                if (out) {
                    char buffer[4096];
                    int  err = UNZ_OK;
                    do {
                        err = unzReadCurrentFile(zip, buffer, sizeof(buffer));
                        if (err > 0) {
                            fwrite(buffer, 1, err, out);
                        }
                    } while (err > 0);
                    fclose(out);
                    extracted_count++;
                    PR_DEBUG("[EPUB_ZIP] Successfully extracted file: %s", full_path);
                } else {
                    PR_ERR("[EPUB_ZIP] Failed to create file: %s", full_path);
                }
#else
                /* For MCU: Use TuyaOS file system API */
                PR_DEBUG("[EPUB_ZIP] Trying to create file: %s", full_path);
                TUYA_FILE out = tkl_fopen(full_path, "wb");
                PR_DEBUG("[EPUB_ZIP] tkl_fopen returned: %p", out);
                
                if (out) {
                    char buffer[4096];
                    int  err = UNZ_OK;
                    int  total_read = 0;
                    int  total_written = 0;
                    
                    do {
                        err = unzReadCurrentFile(zip, buffer, sizeof(buffer));
                        PR_DEBUG("[EPUB_ZIP] unzReadCurrentFile returned: %d bytes", err);
                        
                        if (err > 0) {
                            INT_T written = tkl_fwrite(buffer, err, out);
                            total_read += err;
                            total_written += written;
                            
                            PR_DEBUG("[EPUB_ZIP] tkl_fwrite: wrote %d/%d bytes", written, err);
                            
                            if (written != err) {
                                PR_ERR("[EPUB_ZIP] Write error: expected %d, wrote %d for %s", err, written, full_path);
                                break;
                            }
                        } else if (err < 0) {
                            PR_ERR("[EPUB_ZIP] Read error: %d for %s", err, filename_inzip);
                            break;
                        }
                    } while (err > 0);
                    
                    tkl_fclose(out);
                    extracted_count++;
                    PR_INFO("[EPUB_ZIP] Successfully extracted: %s (read=%d, wrote=%d bytes)", 
                            filename_inzip, total_read, total_written);
                } else {
                    PR_ERR("[EPUB_ZIP] Failed to create file (tkl_fopen returned NULL): %s", full_path);
                }
#endif
                PR_DEBUG("[EPUB_ZIP] Closing current file in ZIP");
                unzCloseCurrentFile(zip);
            } else {
                PR_ERR("[EPUB_ZIP] Failed to open file in ZIP (unzOpenCurrentFile): %s", filename_inzip);
            }
            
            PR_DEBUG("[EPUB_ZIP] Moving to next file...");
        } while (unzGoToNextFile(zip) == UNZ_OK);
        
        PR_DEBUG("[EPUB_ZIP] Finished iterating through ZIP entries");
    } else {
        PR_ERR("[EPUB_ZIP] Failed to go to first file in ZIP");
    }

    unzClose(zip);
    PR_INFO("[EPUB_ZIP] Extraction summary: %d files + %d dirs found, %d files extracted", 
            file_count, dir_count, extracted_count);
    return true;
}

/* Helper: Find OPF file path from container.xml */
static char *find_opf_path(const char *extract_dir)
{
    char container_path[512];
    snprintf(container_path, sizeof(container_path), "%s/META-INF/container.xml", extract_dir);
    
    PR_DEBUG("[EPUB_OPF] Looking for container.xml at: %s", container_path);

    char *container_xml = read_file_contents(container_path, NULL);
    if (!container_xml) {
        PR_ERR("[EPUB_OPF] Failed to read container.xml");
        return NULL;
    }
    
    PR_DEBUG("[EPUB_OPF] container.xml read successfully, length=%d", strlen(container_xml));
    PR_DEBUG("[EPUB_OPF] container.xml content: %.200s...", container_xml); // Show first 200 chars

    /* Parse container.xml to find OPF path */
    char *rootfile = strstr(container_xml, "rootfile");
    if (!rootfile) {
        PR_ERR("[EPUB_OPF] 'rootfile' tag not found in container.xml");
        free(container_xml);
        return NULL;
    }
    
    PR_DEBUG("[EPUB_OPF] Found 'rootfile' tag");

    char *full_path = strstr(rootfile, "full-path=\"");
    if (!full_path) {
        PR_ERR("[EPUB_OPF] 'full-path' attribute not found");
        free(container_xml);
        return NULL;
    }

    full_path += 11; /* Skip "full-path=\"" */
    char *end = strchr(full_path, '"');
    if (!end) {
        PR_ERR("[EPUB_OPF] Closing quote for 'full-path' not found");
        free(container_xml);
        return NULL;
    }

    size_t len      = end - full_path;
    char  *opf_path = (char *)malloc(len + 1);
    strncpy(opf_path, full_path, len);
    opf_path[len] = '\0';
    
    PR_DEBUG("[EPUB_OPF] Extracted OPF relative path: %s", opf_path);

    /* Build full path */
    char *full_opf_path = (char *)malloc(strlen(extract_dir) + len + 2);
    snprintf(full_opf_path, strlen(extract_dir) + len + 2, "%s/%s", extract_dir, opf_path);
    free(opf_path);
    free(container_xml);
    
    PR_DEBUG("[EPUB_OPF] Full OPF path: %s", full_opf_path);

    return full_opf_path;
}

/* Helper: Extract text between XML tags */
static char *extract_xml_tag_content(const char *xml, const char *tag_name)
{
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "<%s", tag_name);

    char *start = strstr(xml, pattern);
    if (!start) {
        return NULL;
    }

    /* Find opening tag end */
    start = strchr(start, '>');
    if (!start) {
        return NULL;
    }
    start++;

    /* Find closing tag */
    char close_pattern[256];
    snprintf(close_pattern, sizeof(close_pattern), "</%s>", tag_name);
    char *end = strstr(start, close_pattern);
    if (!end) {
        return NULL;
    }

    size_t len     = end - start;
    char  *content = (char *)malloc(len + 1);
    strncpy(content, start, len);
    content[len] = '\0';

    return content;
}

/* Helper: Extract attribute value */
static char *extract_xml_attr(const char *xml, const char *tag_name, const char *attr_name)
{
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "<%s", tag_name);

    char *tag = strstr(xml, pattern);
    if (!tag) {
        return NULL;
    }

    char attr_pattern[256];
    snprintf(attr_pattern, sizeof(attr_pattern), "%s=\"", attr_name);
    char *attr = strstr(tag, attr_pattern);
    if (!attr) {
        return NULL;
    }

    attr += strlen(attr_pattern);
    char *end = strchr(attr, '"');
    if (!end) {
        return NULL;
    }

    size_t len   = end - attr;
    char  *value = (char *)malloc(len + 1);
    strncpy(value, attr, len);
    value[len] = '\0';

    return value;
}

/* Parse OPF file to extract metadata and manifest */
static bool parse_opf(epub_reader_t *reader, const char *opf_path)
{
    char *opf_content = read_file_contents(opf_path, NULL);
    if (!opf_content) {
        return false;
    }

    /* Parse metadata */
    epub_metadata_t *meta = (epub_metadata_t *)calloc(1, sizeof(epub_metadata_t));

    char *title = extract_xml_tag_content(opf_content, "dc:title");
    if (title)
        meta->title = title;

    char *author = extract_xml_tag_content(opf_content, "dc:creator");
    if (author)
        meta->author = author;

    char *language = extract_xml_tag_content(opf_content, "dc:language");
    if (language)
        meta->language = language;

    char *publisher = extract_xml_tag_content(opf_content, "dc:publisher");
    if (publisher)
        meta->publisher = publisher;

    char *date = extract_xml_tag_content(opf_content, "dc:date");
    if (date)
        meta->date = date;

    char *identifier = extract_xml_tag_content(opf_content, "dc:identifier");
    if (identifier)
        meta->identifier = identifier;

    reader->metadata = meta;

    /* Parse spine (reading order) */
    char *spine_start = strstr(opf_content, "<spine");
    if (spine_start) {
        /* First pass: count valid itemref entries (those with idref attribute) */
        int   valid_count = 0;
        char *itemref     = strstr(spine_start, "<itemref");
        while (itemref) {
            char *idref = extract_xml_attr((const char *)itemref, "itemref", "idref");
            if (idref) {
                valid_count++;
                free(idref);
            }
            itemref = strstr(itemref + 1, "<itemref");
        }

        reader->spine_count = valid_count;
        if (valid_count > 0) {
            reader->spine_items = (epub_content_item_t **)calloc(valid_count, sizeof(epub_content_item_t *));

            /* Second pass: Extract itemref IDs for valid entries only */
            itemref = strstr(spine_start, "<itemref");
            int idx = 0;
            while (itemref && idx < valid_count) {
                char *idref = extract_xml_attr((const char *)itemref, "itemref", "idref");
                if (idref) {
                    epub_content_item_t *item  = (epub_content_item_t *)calloc(1, sizeof(epub_content_item_t));
                    item->id                   = idref;
                    reader->spine_items[idx++] = item;
                }
                itemref = strstr(itemref + 1, "<itemref");
            }
        }
    }

    /* Parse manifest to get hrefs for items */
    char *manifest_start = strstr(opf_content, "<manifest");
    if (manifest_start) {
        char *item = strstr(manifest_start, "<item");
        while (item) {
            char *id         = extract_xml_attr((const char *)item, "item", "id");
            char *href       = extract_xml_attr((const char *)item, "item", "href");
            char *media_type = extract_xml_attr((const char *)item, "item", "media-type");

            bool assigned = false;
            if (id && href) {
                /* Find matching spine item */
                for (int i = 0; i < reader->spine_count; i++) {
                    if (reader->spine_items[i] && reader->spine_items[i]->id &&
                        strcmp(reader->spine_items[i]->id, id) == 0) {
                        reader->spine_items[i]->href = href;
                        if (media_type) {
                            reader->spine_items[i]->media_type = media_type;
                        }
                        assigned = true;
                        break;
                    }
                }
            }

            /* Always free id - it's only used for comparison, not assigned */
            if (id)
                free(id);

            /* Don't free href and media_type if they were assigned to spine items */
            /* They will be freed when spine items are freed in epub_close() */
            if (href && !assigned)
                free(href);
            if (media_type && !assigned)
                free(media_type);

            item = strstr(item + 1, "<item");
        }
    }

    free(opf_content);
    return true;
}

/* Parse TOC from nav.xhtml or toc.ncx */
static epub_toc_entry_t *parse_toc(epub_reader_t *reader)
{
    char  toc_path[4096];
    char *toc_content = NULL;

    /* First, try to find TOC from OPF directory (same as content files) */
    char  opf_dir[4096];
    char *opf_dir_ptr = strrchr(reader->opf_path, '/');
    if (opf_dir_ptr) {
        size_t dir_len = opf_dir_ptr - reader->opf_path;
        strncpy(opf_dir, reader->opf_path, dir_len);
        opf_dir[dir_len] = '\0';
    } else {
        strncpy(opf_dir, reader->extract_path, sizeof(opf_dir) - 1);
        opf_dir[sizeof(opf_dir) - 1] = '\0';
    }

    /* Try multiple possible TOC file locations */
    const char *toc_candidates[] = {"nav.xhtml", /* EPUB3 standard */
                                    "toc.xhtml", /* Alternative name */
                                    "OEBPS/nav.xhtml", "OEBPS/toc.xhtml",
                                    "toc.ncx", /* EPUB2 */
                                    "OEBPS/toc.ncx",   NULL};

    for (int i = 0; toc_candidates[i] != NULL; i++) {
        /* Try relative to OPF directory */
        snprintf(toc_path, sizeof(toc_path), "%s/%s", opf_dir, toc_candidates[i]);
        if (file_exists(toc_path)) {
            toc_content = read_file_contents(toc_path, NULL);
            if (toc_content) {
                break;
            }
        }

        /* Try relative to extract path */
        snprintf(toc_path, sizeof(toc_path), "%s/%s", reader->extract_path, toc_candidates[i]);
        if (file_exists(toc_path)) {
            toc_content = read_file_contents(toc_path, NULL);
            if (toc_content) {
                break;
            }
        }
    }

    if (!toc_content) {
        return NULL;
    }

    /* Parse TOC - handle both EPUB3 nav.xhtml and EPUB2 toc.ncx */
    epub_toc_entry_t *root    = NULL;
    epub_toc_entry_t *current = NULL;

    /* Check if this is NCX (EPUB2) or XHTML (EPUB3) */
    if (strstr(toc_content, "<ncx") || strstr(toc_content, "<?xml")) {
        /* EPUB2 NCX format */
        char *navmap = strstr(toc_content, "<navMap");
        if (!navmap) {
            navmap = strstr(toc_content, "navMap");
        }

        if (navmap) {
            /* Parse navPoint entries */
            char *navpoint = strstr(navmap, "<navPoint");
            int   level    = 0;

            while (navpoint) {
                char *text_src    = strstr(navpoint, "<text>");
                char *content_src = strstr(navpoint, "<content");

                if (text_src && content_src) {
                    char *text = extract_xml_tag_content((const char *)text_src, "text");
                    char *src  = extract_xml_attr((const char *)content_src, "content", "src");

                    if (text && src) {
                        /* Extract href from src (format: "file.xhtml#fragment" or "file.xhtml") */
                        char *href     = strdup(src);
                        char *fragment = strchr(href, '#');
                        if (fragment) {
                            *fragment = '\0'; /* Remove fragment for now */
                        }

                        epub_toc_entry_t *entry = (epub_toc_entry_t *)calloc(1, sizeof(epub_toc_entry_t));
                        entry->label            = text;
                        entry->href             = href;
                        entry->level            = level;

                        if (!root) {
                            root    = entry;
                            current = entry;
                        } else {
                            current->next = entry;
                            current       = entry;
                        }
                    } else {
                        if (text)
                            free(text);
                        if (src)
                            free(src);
                    }
                }

                /* Find next navPoint */
                navpoint = strstr(navpoint + 1, "<navPoint");
            }
        }
    } else {
        /* EPUB3 XHTML format - look for nav element with epub:type="toc" */
        char *nav_start = strstr(toc_content, "<nav");
        if (!nav_start) {
            nav_start = strstr(toc_content, "<ol");
        }
        if (!nav_start) {
            nav_start = strstr(toc_content, "<li");
        }

        if (nav_start) {
            /* Find the TOC nav (epub:type="toc") */
            char *toc_nav = nav_start;
            while (toc_nav) {
                if (strstr(toc_nav, "epub:type=\"toc\"") || strstr(toc_nav, "epub:type='toc'") ||
                    strstr(toc_nav, "role=\"doc-toc\"")) {
                    break;
                }
                toc_nav = strstr(toc_nav + 1, "<nav");
            }

            if (!toc_nav) {
                toc_nav = nav_start; /* Fallback to first nav */
            }

            /* Parse nested list structure */
            int   level = 0;
            char *pos   = toc_nav;

            while (pos) {
                /* Look for <a> tags with href */
                char *link = strstr(pos, "<a");
                if (!link)
                    break;

                /* Check if this link is inside a <li> */
                char *li_start = strrchr(toc_nav, '<');
                if (li_start && li_start < link) {
                    /* Count nested <ol> or <ul> to determine level */
                    level       = 0;
                    char *check = toc_nav;
                    while (check < link) {
                        if (strncmp(check, "<ol", 3) == 0 || strncmp(check, "<ul", 3) == 0) {
                            level++;
                        } else if (strncmp(check, "</ol", 4) == 0 || strncmp(check, "</ul", 4) == 0) {
                            if (level > 0)
                                level--;
                        }
                        check++;
                    }
                }

                char *href = extract_xml_attr((const char *)link, "a", "href");
                char *text = extract_xml_tag_content((const char *)link, "a");

                if (href && text) {
                    epub_toc_entry_t *entry = (epub_toc_entry_t *)calloc(1, sizeof(epub_toc_entry_t));
                    entry->label            = text;
                    entry->href             = href;
                    entry->level            = level;

                    if (!root) {
                        root    = entry;
                        current = entry;
                    } else {
                        current->next = entry;
                        current       = entry;
                    }
                } else {
                    if (href)
                        free(href);
                    if (text)
                        free(text);
                }

                pos = strstr(link + 1, "<a");
            }
        }
    }

    free(toc_content);
    return root;
}

/* Strip HTML tags from content - simple version that just removes tags */
static char *strip_html_tags(const char *html)
{
    if (!html)
        return NULL;

    size_t len  = strlen(html);
    char  *text = (char *)malloc(len + 1);
    if (!text)
        return NULL;

    int in_tag  = 0;
    int out_idx = 0;

    for (size_t i = 0; i < len; i++) {
        if (html[i] == '<') {
            in_tag = 1;
        } else if (html[i] == '>') {
            in_tag = 0;
        } else if (!in_tag) {
            /* Filter CSS at-rules like @page, @media, @import, etc. */
            if (html[i] == '@') {
                /* Skip CSS at-rule until closing brace */
                // size_t start = i;  /* Unused variable */
                i++; /* Skip '@' */

                /* Skip the at-rule name (page, media, import, etc.) */
                while (i < len && (isalnum((unsigned char)html[i]) || html[i] == '-' || html[i] == '_')) {
                    i++;
                }

                /* Skip whitespace */
                while (i < len && isspace((unsigned char)html[i])) {
                    i++;
                }

                /* Skip until we find the closing brace */
                int brace_count = 0;
                while (i < len) {
                    if (html[i] == '{') {
                        brace_count++;
                    } else if (html[i] == '}') {
                        brace_count--;
                        if (brace_count == 0) {
                            i++; /* Skip the closing brace */
                            break;
                        }
                    }
                    i++;
                }
                i--;      /* Adjust for loop increment */
                continue; /* Skip this CSS block */
            }

            /* Filter CSS rule blocks like "body { ... }" or "Cover body { ... }" */
            /* Look for pattern: word(s) followed by { with CSS properties inside */
            if (html[i] == '{') {
                /* Check if this looks like a CSS rule block by looking ahead for property: value pattern */
                size_t check_pos = i + 1;
                while (check_pos < len && isspace((unsigned char)html[check_pos])) {
                    check_pos++;
                }

                /* Look for CSS property pattern: word followed by colon */
                int looks_like_css = 0;
                if (check_pos < len) {
                    size_t prop_start = check_pos;
                    /* Check for valid CSS property name characters */
                    while (prop_start < len && (isalnum((unsigned char)html[prop_start]) || html[prop_start] == '-' ||
                                                html[prop_start] == '_')) {
                        prop_start++;
                    }

                    /* If we found a word followed by colon, it's likely CSS */
                    if (prop_start < len && html[prop_start] == ':') {
                        looks_like_css = 1;
                    }
                }

                if (looks_like_css) {
                    /* Remove any CSS selector that was already output before the { */
                    /* Look backwards in output to find selector pattern */
                    int selector_end = out_idx;
                    /* Remove trailing whitespace */
                    while (selector_end > 0 && isspace((unsigned char)text[selector_end - 1])) {
                        selector_end--;
                    }

                    /* Look backwards for CSS selector pattern (words, spaces, commas, dots, hashes) */
                    int selector_start = selector_end;
                    while (selector_start > 0 && (isalnum((unsigned char)text[selector_start - 1]) ||
                                                  text[selector_start - 1] == ' ' || text[selector_start - 1] == ',' ||
                                                  text[selector_start - 1] == '.' || text[selector_start - 1] == '#' ||
                                                  text[selector_start - 1] == '-' || text[selector_start - 1] == '_')) {
                        selector_start--;
                    }

                    /* If we found a selector pattern, remove it from output */
                    if (selector_start < selector_end) {
                        out_idx = selector_start;
                    }

                    /* Skip the entire CSS rule block */
                    int brace_count = 1;
                    i++; /* Skip opening { */
                    while (i < len && brace_count > 0) {
                        if (html[i] == '{') {
                            brace_count++;
                        } else if (html[i] == '}') {
                            brace_count--;
                            if (brace_count == 0) {
                                i++; /* Skip the closing brace */
                                break;
                            }
                        }
                        i++;
                    }
                    i--;      /* Adjust for loop increment */
                    continue; /* Skip this CSS block */
                }
            }

            /* Convert HTML entities */
            if (html[i] == '&') {
                if (i + 5 < len && strncmp(&html[i], "&amp;", 5) == 0) {
                    text[out_idx++] = '&';
                    i += 4;
                } else if (i + 4 < len && strncmp(&html[i], "&lt;", 4) == 0) {
                    text[out_idx++] = '<';
                    i += 3;
                } else if (i + 4 < len && strncmp(&html[i], "&gt;", 4) == 0) {
                    text[out_idx++] = '>';
                    i += 3;
                } else if (i + 6 < len && strncmp(&html[i], "&quot;", 6) == 0) {
                    text[out_idx++] = '"';
                    i += 5;
                } else if (i + 6 < len && strncmp(&html[i], "&apos;", 6) == 0) {
                    text[out_idx++] = '\'';
                    i += 5;
                } else if (i + 5 < len && strncmp(&html[i], "&nbsp;", 6) == 0) {
                    text[out_idx++] = ' ';
                    i += 5;
                } else {
                    text[out_idx++] = html[i];
                }
            } else {
                text[out_idx++] = html[i];
            }
        }
    }

    text[out_idx] = '\0';

    /* Collapse whitespace */
    char *cleaned     = (char *)malloc(out_idx + 1);
    int   cleaned_idx = 0;
    int   prev_space  = 0;

    for (int i = 0; i < out_idx; i++) {
        if (isspace((unsigned char)text[i])) {
            if (!prev_space && cleaned_idx > 0) {
                cleaned[cleaned_idx++] = ' ';
            }
            prev_space = 1;
        } else {
            cleaned[cleaned_idx++] = text[i];
            prev_space             = 0;
        }
    }
    cleaned[cleaned_idx] = '\0';

    free(text);
    return cleaned;
}

/* Public API implementations */

epub_reader_t *epub_open(const char *filepath)
{
    PR_DEBUG("[EPUB_OPEN] Start: filepath=%s", filepath ? filepath : "(null)");
    
    if (!filepath) {
        PR_ERR("[EPUB_OPEN] Invalid filepath");
        return NULL;
    }

    epub_reader_t *reader = (epub_reader_t *)calloc(1, sizeof(epub_reader_t));
    if (!reader) {
        PR_ERR("[EPUB_OPEN] Failed to allocate reader memory");
        return NULL;
    }
    
    PR_DEBUG("[EPUB_OPEN] Reader allocated: %p", reader);

    reader->filepath = strdup(filepath);
    reader->config   = epub_get_default_config();
    /* Set default font to LVGL default if not set */
    if (!reader->config.font_path) {
        reader->config.font_path = strdup("lv_font_montserrat_14");
    }

    /* Initialize progress to -1 (invalid) to detect if cache was loaded */
    reader->current_chapter     = -1;
    reader->current_page        = -1;
    reader->current_char_offset = -1;

    /* Ensure cache directory exists */
    char *cache_dir = get_cache_dir();
    PR_DEBUG("[EPUB_OPEN] Cache directory: %s", cache_dir);
    
    if (ensure_cache_dir()) {
        PR_DEBUG("[EPUB_OPEN] Cache directory ensured");
    }

    /* Try to load from cache first */
    bool cache_loaded = load_cache(reader);
    if (cache_loaded && reader->current_chapter >= 0) {
        PR_INFO("[EPUB_OPEN] Loaded from cache. Chapter %d, Page %d, Offset %d",
               reader->current_chapter, reader->current_page, reader->current_char_offset);
    } else {
        /* No cache or invalid cache - initialize to 0 */
        reader->current_chapter     = 0;
        reader->current_page        = 0;
        reader->current_char_offset = 0;
        PR_DEBUG("[EPUB_OPEN] No cache, initialized to chapter 0");
    }

    /* Create temporary extraction directory */
    char extract_dir[256];  /* Reduced size for embedded systems */
#ifdef __linux__
    snprintf(extract_dir, sizeof(extract_dir), "/tmp/epub_%ld", (long)getpid());
#else
    /* For MCU: Use SD card path with hash-based subdirectory to avoid path length issues */
    /* Generate short hash from filepath to avoid long path names */
    unsigned int hash = 0;
    for (const char *p = filepath; *p; p++) {
        hash = hash * 31 + (unsigned char)(*p);
    }
    
    /* Use short hash-based directory name */
    snprintf(extract_dir, sizeof(extract_dir), "/sdcard/.cache/epub_%08x", hash);
#endif
    reader->extract_path = strdup(extract_dir);
    
    PR_DEBUG("[EPUB_OPEN] Extract directory: %s", extract_dir);

    /* Check if already extracted (cache optimization) */
    bool already_extracted = false;
    char container_xml_path[384];  /* 256 + 128 for subdirs */
    snprintf(container_xml_path, sizeof(container_xml_path), "%s/META-INF/container.xml", extract_dir);
    
    BOOL_T dir_exists = FALSE;
    BOOL_T container_exists = FALSE;
    tkl_fs_is_exist(extract_dir, &dir_exists);
    tkl_fs_is_exist(container_xml_path, &container_exists);
    
    if (dir_exists && container_exists) {
        PR_INFO("[EPUB_OPEN] ✅ Found extracted cache, skipping ZIP extraction");
        already_extracted = true;
    } else {
        PR_DEBUG("[EPUB_OPEN] No cache found (dir_exist=%d, container_exist=%d), extracting...", 
                 dir_exists, container_exists);
    }

    /* Extract EPUB (ZIP) file only if not already extracted */
    if (!already_extracted) {
        PR_DEBUG("[EPUB_OPEN] Starting ZIP extraction...");
        if (!extract_zip(filepath, extract_dir)) {
            PR_ERR("[EPUB_OPEN] Failed to extract ZIP");
            epub_close(reader);
            return NULL;
        }
        PR_INFO("[EPUB_OPEN] ✅ ZIP extraction completed");
    }

    if (!cache_loaded) {
        /* Cache not found or invalid, parse normally */
        PR_DEBUG("[EPUB_OPEN] No cache, parsing EPUB structure...");
        
        /* Find OPF file */
        PR_DEBUG("[EPUB_OPEN] Finding OPF file in %s", extract_dir);
        reader->opf_path = find_opf_path(extract_dir);
        
        if (!reader->opf_path) {
            PR_ERR("[EPUB_OPEN] Failed to find OPF file");
            epub_close(reader);
            return NULL;
        }
        
        PR_DEBUG("[EPUB_OPEN] OPF file found: %s", reader->opf_path);

        /* Parse OPF */
        PR_DEBUG("[EPUB_OPEN] Parsing OPF file...");
        if (!parse_opf(reader, reader->opf_path)) {
            PR_ERR("[EPUB_OPEN] Failed to parse OPF file");
            epub_close(reader);
            return NULL;
        }
        
        PR_DEBUG("[EPUB_OPEN] OPF parsed successfully, spine_count=%d", reader->spine_count);

        /* Parse TOC */
        PR_DEBUG("[EPUB_OPEN] Parsing TOC...");
        reader->toc = parse_toc(reader);
        PR_DEBUG("[EPUB_OPEN] TOC parsed, toc=%p", reader->toc);

        /* Save cache for next time */
        PR_DEBUG("[EPUB_OPEN] Saving cache...");
        save_cache(reader);
        PR_DEBUG("[EPUB_OPEN] Cache saved");
    } else {
        /* Cache loaded successfully, but we still need OPF path for content access */
        /* Update OPF path to point to extracted directory */
        if (reader->opf_path) {
            /* Extract just the filename from cached OPF path */
            const char *opf_filename = strrchr(reader->opf_path, '/');
            if (!opf_filename) {
                opf_filename = strrchr(reader->opf_path, '\\');
            }
            if (opf_filename) {
                opf_filename++;
            } else {
                opf_filename = reader->opf_path;
            }

            free(reader->opf_path);
            size_t path_len  = strlen(extract_dir) + strlen(opf_filename) + 2;
            reader->opf_path = (char *)malloc(path_len);
            if (reader->opf_path) {
                snprintf(reader->opf_path, path_len, "%s/%s", extract_dir, opf_filename);
            } else {
                /* Fallback: find OPF path */
                reader->opf_path = find_opf_path(extract_dir);
            }
        } else {
            /* No OPF path in cache, find it */
            reader->opf_path = find_opf_path(extract_dir);
        }

        /* Verify cache is still valid by checking if OPF file exists */
        if (!reader->opf_path || !file_exists(reader->opf_path)) {
            /* Cache is stale, reparse */
            if (reader->opf_path) {
                free(reader->opf_path);
            }
            reader->opf_path = find_opf_path(extract_dir);
            if (!reader->opf_path) {
                epub_close(reader);
                return NULL;
            }
            /* Clear cached data and reparse */
            if (reader->metadata) {
                epub_free_metadata(reader->metadata);
                reader->metadata = NULL;
            }
            if (reader->toc) {
                epub_free_toc(reader->toc);
                reader->toc = NULL;
            }
            if (reader->spine_items) {
                for (int i = 0; i < reader->spine_count; i++) {
                    if (reader->spine_items[i]) {
                        epub_free_content_item(reader->spine_items[i]);
                    }
                }
                free(reader->spine_items);
                reader->spine_items = NULL;
                reader->spine_count = 0;
            }

            if (!parse_opf(reader, reader->opf_path)) {
                epub_close(reader);
                return NULL;
            }
            reader->toc = parse_toc(reader);
            save_cache(reader);
        }
    }

    reader->is_open = true;
    return reader;
}

void epub_close(epub_reader_t *reader)
{
    if (!reader)
        return;

    /* Free font path */
    if (reader->config.font_path) {
        free(reader->config.font_path);
        reader->config.font_path = NULL;
    }

    /* Free metadata */
    if (reader->metadata) {
        epub_free_metadata(reader->metadata);
    }

    /* Free TOC */
    if (reader->toc) {
        epub_free_toc(reader->toc);
    }

    /* Free spine items */
    if (reader->spine_items) {
        for (int i = 0; i < reader->spine_count; i++) {
            if (reader->spine_items[i]) {
                epub_free_content_item(reader->spine_items[i]);
            }
        }
        free(reader->spine_items);
    }

    /* Clean up extraction directory */
    if (reader->extract_path) {
        remove_directory(reader->extract_path);
        free(reader->extract_path);
    }

    if (reader->opf_path)
        free(reader->opf_path);
    if (reader->filepath)
        free(reader->filepath);

    free(reader);
}

epub_metadata_t *epub_get_metadata(epub_reader_t *reader)
{
    if (!reader || !reader->metadata) {
        return NULL;
    }
    return reader->metadata;
}

void epub_free_metadata(epub_metadata_t *metadata)
{
    if (!metadata)
        return;

    if (metadata->title)
        free(metadata->title);
    if (metadata->author)
        free(metadata->author);
    if (metadata->language)
        free(metadata->language);
    if (metadata->publisher)
        free(metadata->publisher);
    if (metadata->date)
        free(metadata->date);
    if (metadata->identifier)
        free(metadata->identifier);

    free(metadata);
}

epub_toc_entry_t *epub_get_toc(epub_reader_t *reader)
{
    if (!reader)
        return NULL;
    return reader->toc;
}

void epub_free_toc(epub_toc_entry_t *toc)
{
    if (!toc)
        return;

    if (toc->label)
        free(toc->label);
    if (toc->href)
        free(toc->href);

    if (toc->next)
        epub_free_toc(toc->next);
    if (toc->children)
        epub_free_toc(toc->children);

    free(toc);
}

epub_content_item_t *epub_get_content_item(epub_reader_t *reader, const char *item_id)
{
    if (!reader || !item_id)
        return NULL;

    for (int i = 0; i < reader->spine_count; i++) {
        if (reader->spine_items[i] && reader->spine_items[i]->id && strcmp(reader->spine_items[i]->id, item_id) == 0) {
            return reader->spine_items[i];
        }
    }

    return NULL;
}

epub_content_item_t *epub_get_content_by_href(epub_reader_t *reader, const char *href)
{
    if (!reader || !href)
        return NULL;

    for (int i = 0; i < reader->spine_count; i++) {
        if (reader->spine_items[i] && reader->spine_items[i]->href && strcmp(reader->spine_items[i]->href, href) == 0) {
            return reader->spine_items[i];
        }
    }

    return NULL;
}

void epub_free_content_item(epub_content_item_t *item)
{
    if (!item)
        return;

    if (item->id)
        free(item->id);
    if (item->href)
        free(item->href);
    if (item->media_type)
        free(item->media_type);
    if (item->content)
        free(item->content);

    free(item);
}

char *epub_get_text_content(epub_reader_t *reader, const char *href)
{
    if (!reader || !href) {
        PR_ERR("[EPUB_GET_TEXT] Invalid parameters: reader=%p, href=%p", reader, href);
        return NULL;
    }

    /* Build full path - use PSRAM allocation to avoid stack overflow (4KB is too much for embedded systems) */
    char *full_path = (char *)tal_psram_malloc(512);
    char *opf_dir = (char *)tal_psram_malloc(512);
    
    if (!full_path || !opf_dir) {
        PR_ERR("[EPUB_GET_TEXT] Failed to allocate path buffers from PSRAM");
        if (full_path) tal_psram_free(full_path);
        if (opf_dir) tal_psram_free(opf_dir);
        return NULL;
    }

    /* Extract directory from OPF path */
    if (reader->opf_path && strlen(reader->opf_path) > 0) {
        char *opf_dir_ptr = strrchr(reader->opf_path, '/');
        if (opf_dir_ptr) {
            size_t dir_len = opf_dir_ptr - reader->opf_path;
            if (dir_len >= 512) dir_len = 511;
            strncpy(opf_dir, reader->opf_path, dir_len);
            opf_dir[dir_len] = '\0';
            snprintf(full_path, 512, "%s/%s", opf_dir, href);
        } else {
            /* Fallback: use extract path */
            snprintf(full_path, 512, "%s/%s", reader->extract_path, href);
        }
    } else {
        /* opf_path is NULL or empty, use extract path */
        PR_DEBUG("[EPUB_GET_TEXT] opf_path is NULL/empty, using extract_path");
        snprintf(full_path, 512, "%s/%s", reader->extract_path, href);
    }

    PR_DEBUG("[EPUB_GET_TEXT] Trying to read: %s", full_path);
    char *html_content = read_file_contents(full_path, NULL);
    if (!html_content) {
        /* Try alternative paths */
        if (reader->extract_path) {
            snprintf(full_path, 512, "%s/OEBPS/%s", reader->extract_path, href);
            PR_DEBUG("[EPUB_GET_TEXT] Trying alternative path: %s", full_path);
            html_content = read_file_contents(full_path, NULL);
            if (!html_content) {
                snprintf(full_path, 512, "%s/%s", reader->extract_path, href);
                PR_DEBUG("[EPUB_GET_TEXT] Trying fallback path: %s", full_path);
                html_content = read_file_contents(full_path, NULL);
            }
        }
    }
    
    /* Free path buffers */
    tal_psram_free(full_path);
    tal_psram_free(opf_dir);

    if (!html_content) {
        PR_ERR("[EPUB_GET_TEXT] Failed to read content for href: %s", href);
        return NULL;
    }
    
    PR_DEBUG("[EPUB_GET_TEXT] Successfully read content, size=%zu", strlen(html_content));

    char *text = strip_html_tags(html_content);
    free(html_content);

    return text;
}

size_t epub_calculate_text_size(const char *text, const epub_config_t *config)
{
    if (!text || !config)
        return 0;
    return strlen(text);
}

int epub_calculate_pages(const char *text, const epub_config_t *config)
{
    if (!text || !config)
        return 0;

    size_t text_len = strlen(text);
    int    pages    = (text_len + config->chars_per_page - 1) / config->chars_per_page;
    return pages > 0 ? pages : 1;
}

int epub_get_total_pages(epub_reader_t *reader, const epub_config_t *config)
{
    if (!reader || !config)
        return 0;

    int total_pages = 0;

    for (int i = 0; i < reader->spine_count; i++) {
        if (reader->spine_items[i] && reader->spine_items[i]->href) {
            char *text = epub_get_text_content(reader, reader->spine_items[i]->href);
            if (text) {
                total_pages += epub_calculate_pages(text, config);
                free(text);
            }
        }
    }

    return total_pages;
}

int epub_get_page_index(epub_reader_t *reader, const char *href, int char_offset, const epub_config_t *config)
{
    if (!reader || !href || !config)
        return 0;

    int page_index = 0;

    /* Find the item in spine */
    for (int i = 0; i < reader->spine_count; i++) {
        if (reader->spine_items[i] && reader->spine_items[i]->href) {
            if (strcmp(reader->spine_items[i]->href, href) == 0) {
                /* Found the item, calculate page within it */
                page_index += char_offset / config->chars_per_page;
                break;
            } else {
                /* Add pages from previous items */
                char *text = epub_get_text_content(reader, reader->spine_items[i]->href);
                if (text) {
                    page_index += epub_calculate_pages(text, config);
                    free(text);
                }
            }
        }
    }

    return page_index;
}

float epub_get_progress(epub_reader_t *reader, const char *href, int char_offset, const epub_config_t *config)
{
    if (!reader || !href || !config)
        return 0.0f;

    int current_page = epub_get_page_index(reader, href, char_offset, config);
    int total_pages  = epub_get_total_pages(reader, config);

    if (total_pages == 0)
        return 0.0f;

    return ((float)current_page / (float)total_pages) * 100.0f;
}

void epub_set_config(epub_reader_t *reader, const epub_config_t *config)
{
    if (!reader || !config)
        return;
    reader->config = *config;
}

/* Save reading progress to cache */
void epub_save_progress(epub_reader_t *reader, int chapter, int page, int char_offset)
{
    if (!reader)
        return;

    reader->current_chapter     = chapter;
    reader->current_page        = page;
    reader->current_char_offset = char_offset;

    /* Save to cache file */
    save_cache(reader);
}

/* Get reading progress */
void epub_get_progress_status(epub_reader_t *reader, int *chapter, int *page, int *char_offset)
{
    if (!reader) {
        if (chapter)
            *chapter = 0;
        if (page)
            *page = 0;
        if (char_offset)
            *char_offset = 0;
        return;
    }

    if (chapter)
        *chapter = reader->current_chapter;
    if (page)
        *page = reader->current_page;
    if (char_offset)
        *char_offset = reader->current_char_offset;
}

char *epub_parse_xml_text(const char *xml, const char *tag_name)
{
    if (!xml)
        return NULL;

    if (tag_name) {
        return extract_xml_tag_content(xml, tag_name);
    } else {
        /* Extract all text content */
        return strip_html_tags(xml);
    }
}

char *epub_parse_xml_attr(const char *xml, const char *tag_name, const char *attr_name)
{
    if (!xml || !tag_name || !attr_name)
        return NULL;
    return extract_xml_attr(xml, tag_name, attr_name);
}

epub_content_item_t **epub_get_spine_items(epub_reader_t *reader, int *count)
{
    if (!reader || !count)
        return NULL;

    *count = reader->spine_count;
    return reader->spine_items;
}
