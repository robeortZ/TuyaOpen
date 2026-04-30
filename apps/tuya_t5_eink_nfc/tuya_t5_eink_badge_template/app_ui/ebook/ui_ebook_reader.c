/**
 * @file ui_ebook_reader.c
 * @brief LVGL-based EPUB/TXT ebook reader implementation
 */

#include "ui_ebook_reader.h"
#include "../ui.h"
#include "../screens/ui_file_screen.h"
// #include "../../lvgl/src/font/binfont_loader/lv_binfont_loader.h"
// #include "lv_binfont_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "tal_log.h"  // TuyaOS log API
#include "lv_vendor.h"  // For LVGL thread locking

#define FS_PATH_BASE "/sdcard"
#define CACHE_DIR FS_PATH_BASE "/sdcard/.cache/epub_reader"
#define FONT_DIR FS_PATH_BASE "/sdcard/fonts"
/* Use LVGL filesystem path with 'A:' prefix (LV_FS_STDIO_LETTER) */
/* The path after 'A:' is relative to LV_FS_STDIO_PATH */
#define DEFAULT_FONT "/sdcard/fonts/LXGWWenKaiMonoLite-Regular-14px.bin"

/* Default display settings */
#define DEFAULT_MARGIN_LEFT    20
#define DEFAULT_MARGIN_RIGHT   20
#define DEFAULT_MARGIN_TOP     5
#define DEFAULT_MARGIN_BOTTOM  18
#define DEFAULT_LETTER_SPACING 1

/* Initialize display settings with defaults */
static void init_display_settings(ui_ebook_display_settings_t *settings)
{
    if (!settings) return;

    /* Initialize font path */
    strncpy(settings->font_path, DEFAULT_FONT, sizeof(settings->font_path) - 1);
    settings->font_path[sizeof(settings->font_path) - 1] = '\0';

    /* Initialize font - font size is determined by the loaded font binary */
    settings->font = NULL;

    /* Initialize spacing - use TIGHT for minimal spacing */
    settings->line_spacing = UI_EBOOK_LINE_SPACING_TIGHT;
    settings->letter_spacing = DEFAULT_LETTER_SPACING;
    settings->line_height_mult = 1.0f;

    /* Initialize margins */
    settings->margin_left = DEFAULT_MARGIN_LEFT;
    settings->margin_right = DEFAULT_MARGIN_RIGHT;
    settings->margin_top = DEFAULT_MARGIN_TOP;
    settings->margin_bottom = DEFAULT_MARGIN_BOTTOM;

    /* Initialize text area (will be calculated) */
    settings->text_area_width = 0;
    settings->text_area_height = 0;

    /* Initialize calculated values */
    settings->line_height = 0;
    settings->line_spacing_px = 0;
}

/* Update calculated display values based on current settings */
static void update_display_calculations(ui_ebook_display_settings_t *settings)
{
    if (!settings || !settings->font) return;

    /* Calculate line height multiplier based on spacing mode - made much tighter */
    switch (settings->line_spacing) {
        case UI_EBOOK_LINE_SPACING_TIGHT:
            settings->line_height_mult = 1.0f;  /* No extra spacing */
            break;
        case UI_EBOOK_LINE_SPACING_MID:
            settings->line_height_mult = 1.2f;  /* Medium spacing */
            break;
        case UI_EBOOK_LINE_SPACING_LOOSE:
            settings->line_height_mult = 1.5f;  /* Loose spacing */
            break;
    }

    /* Calculate actual line height and spacing */
    settings->line_height = (int32_t)(settings->font->line_height * settings->line_height_mult);
    settings->line_spacing_px = settings->line_height - settings->font->line_height;
}


/* Forward declarations */
static int find_next_page_end(ui_ebook_reader_t *reader, int start_offset);
static int find_prev_page_start(ui_ebook_reader_t *reader, int start_offset);
static bool load_chapter(ui_ebook_reader_t *reader, int chapter_idx);
static char *read_txt_file(const char *filepath);
static void update_text_style(ui_ebook_reader_t *reader);
static void calculate_text_area(ui_ebook_reader_t *reader);
static void create_settings_page(ui_ebook_reader_t *reader);
static void update_settings_stats(ui_ebook_reader_t *reader);
static void update_epub_config_font_path(ui_ebook_reader_t *reader);
static void save_lvgl_config_to_cache(ui_ebook_reader_t *reader);
static void settings_menu_item_event_cb(lv_event_t *e);
static void create_font_selection_page(ui_ebook_reader_t *reader);
static void scan_font_directory(ui_ebook_reader_t *reader);
static void font_selection_item_event_cb(lv_event_t *e);
static void font_selection_handle_key(ui_ebook_reader_t *reader, lv_key_t key);
static int calculate_text_width(const char *text, int len, const lv_font_t *font, int letter_spacing);
static int calculate_lines_for_text(const char *text, int len, int32_t width, const lv_font_t *font, int letter_spacing, float line_height_mult);
static void build_page_cache(ui_ebook_reader_t *reader);
static void free_page_cache(ui_ebook_reader_t *reader);
static bool save_page_cache(ui_ebook_reader_t *reader, const char *filepath);
static bool load_page_cache(ui_ebook_reader_t *reader, const char *filepath);
static void draw_character_boxes(ui_ebook_reader_t *reader, const char *text, int text_len);
static void clear_character_boxes(ui_ebook_reader_t *reader);
static void create_settings_page(ui_ebook_reader_t *reader);
static void update_settings_stats(ui_ebook_reader_t *reader);
static void settings_menu_item_event_cb(lv_event_t *e);

/* Calculate text width in pixels for given text length */
static int calculate_text_width(const char *text, int len, const lv_font_t *font, int letter_spacing)
{
    if (!text || !font || len <= 0) {
        return 0;
    }

    int32_t width = 0;
    lv_font_glyph_dsc_t g_dsc;
    for (int i = 0; i < len && text[i] != '\0'; i++) {
        uint32_t unicode = (unsigned char)text[i];
        uint32_t next_unicode = (i + 1 < len && text[i + 1] != '\0') ? (unsigned char)text[i + 1] : 0;
        if (font->get_glyph_dsc && font->get_glyph_dsc(font, &g_dsc, unicode, next_unicode)) {
            width += g_dsc.adv_w;
            if (i < len - 1 && !isspace((unsigned char)text[i])) {
                width += letter_spacing;
            }
        }
    }
    return width;
}

/* Calculate number of lines needed for text given width constraint */
__attribute__((unused)) static int calculate_lines_for_text(const char *text, int len, int32_t max_width, const lv_font_t *font, int letter_spacing, float line_height_mult)
{
    (void)line_height_mult; // Currently unused but kept for future use
    if (!text || !font || len <= 0 || max_width <= 0) {
        return 1;
    }

    int lines = 1;
    int line_start = 0;
    int last_space = -1;
    int32_t line_width = 0;

    for (int i = 0; i < len && text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            lines++;
            line_start = i + 1;
            line_width = 0;
            last_space = -1;
            continue;
        }

        uint32_t unicode = (unsigned char)text[i];
        uint32_t next_unicode = (i + 1 < len && text[i + 1] != '\0') ? (unsigned char)text[i + 1] : 0;
        lv_font_glyph_dsc_t g_dsc;
        if (font->get_glyph_dsc && font->get_glyph_dsc(font, &g_dsc, unicode, next_unicode)) {
            int char_width = g_dsc.adv_w;
            if (i < len - 1 && !isspace((unsigned char)text[i])) {
                char_width += letter_spacing;
            }

            if (isspace((unsigned char)text[i])) {
                last_space = i;
            }

            if (line_width + char_width > max_width && line_width > 0) {
                /* Need to wrap */
                if (last_space >= line_start) {
                    /* Wrap at last space */
                    i = last_space;
                    line_start = last_space + 1;
                } else {
                    /* No space found, wrap here */
                    line_start = i;
                }
                lines++;
                line_width = 0;
                last_space = -1;
            } else {
                line_width += char_width;
            }
        }
    }

    return lines;
}

/* Calculate text area dimensions - accounts for all margins, padding, and spacing */
static void calculate_text_area(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->text_label) {
        return;
    }

    /* Force layout update to ensure all styles are applied */
    lv_obj_update_layout(reader->text_label);

    /* Get the container to calculate available space */
    lv_obj_t *container = lv_obj_get_parent(reader->text_label);
    if (!container) {
        return;
    }

    /* Get the label's content area - this already accounts for padding (margins) */
    /* The content area is the space inside the label where text can be rendered */
    int32_t label_content_width = lv_obj_get_content_width(reader->text_label);
    int32_t label_content_height = lv_obj_get_content_height(reader->text_label);

    /* Get padding values (margins) to ensure they're properly accounted for */
    // int32_t pad_top = lv_obj_get_style_pad_top(reader->text_label, LV_PART_MAIN);
    // int32_t pad_bottom = lv_obj_get_style_pad_bottom(reader->text_label, LV_PART_MAIN);

    /* Use content width directly - it already accounts for left/right padding */
    reader->display.text_area_width = label_content_width;
    if (reader->display.text_area_width < 0) {
        reader->display.text_area_width = 0;
    }

    /* Calculate available text area height
     * The content height already accounts for top and bottom padding (margins)
     * But we also need to subtract the progress bar height if it exists */
    int32_t progress_bar_height = 0;
    if (reader->progress_line) {
        progress_bar_height = lv_obj_get_height(reader->progress_line);
    }

    /* Text area height = label content height - progress bar height
     * The content height already excludes top and bottom padding (margins) */
    reader->display.text_area_height = label_content_height - progress_bar_height;

    /* Ensure top and bottom margins are strictly enforced
     * The content height should already account for these, but verify */
    if (reader->display.text_area_height < 0) {
        reader->display.text_area_height = 0;
    }

    /* Ensure we respect the fixed text box boundary
     * The text area should not exceed the available content space */
    if (reader->display.text_area_height > label_content_height) {
        reader->display.text_area_height = label_content_height;
    }

    /* Ensure we have valid dimensions */
    if (reader->display.text_area_width <= 0) {
        reader->display.text_area_width = 100; /* Fallback */
    }
    if (reader->display.text_area_height <= 0) {
        reader->display.text_area_height = 100; /* Fallback */
    }

    /* Update calculated values */
    update_display_calculations(&reader->display);
}

/* Update epub config font_path with current font path from display settings */
static void update_epub_config_font_path(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->reader || !reader->display.font_path[0]) {
        return;
    }

    /* Free old font_path if it exists */
    if (reader->config.font_path) {
        free(reader->config.font_path);
        reader->config.font_path = NULL;
    }

    /* Allocate and copy current font path */
    reader->config.font_path = strdup(reader->display.font_path);

    /* Update epub reader's config */
    epub_set_config(reader->reader, &reader->config);
}

/* Save LVGL display settings to cache */
static void save_lvgl_config_to_cache(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->reader || !reader->display.font_path[0]) {
        return;
    }

    /* Calculate text area if not already calculated */
    calculate_text_area(reader);

    /* Save LVGL display configuration to cache */
    epub_save_lvgl_config(
        reader->reader,
        reader->display.font_path,
        reader->display.line_spacing,
        reader->display.letter_spacing,
        reader->display.margin_left,
        reader->display.margin_right,
        reader->display.margin_top,
        reader->display.margin_bottom,
        reader->display.text_area_width,
        reader->display.text_area_height,
        reader->display.line_height,
        reader->display.line_spacing_px
    );
}

/* Find next page end using screen-based calculation */
static int find_next_page_end(ui_ebook_reader_t *reader, int start_offset)
{
    if (!reader || !reader->current_text || !reader->display.font) {
        return start_offset;
    }

    if (start_offset >= (int)reader->current_text_len) {
        return reader->current_text_len;
    }

    calculate_text_area(reader);
    if (reader->display.text_area_width <= 0 || reader->display.text_area_height <= 0) {
        return start_offset;
    }

    const lv_font_t *font = reader->display.font;
    int32_t letter_spacing = reader->display.letter_spacing;
    int32_t line_height = reader->display.line_height;
    int32_t line_spacing = reader->display.line_spacing_px;

    /* Calculate maximum lines that can fit
     * For N lines, total height = N * line_height - line_spacing
     * (The last line doesn't need spacing below it)
     * Solving for N: N = (text_area_height + line_spacing) / line_height
     * Use ceiling division to be more generous with space usage */
    int max_lines;
    if (line_height > 0) {
        /* Use ceiling division to allow using more of the available space */
        /* Add line_height-1 before dividing to round up */
        max_lines = (reader->display.text_area_height + line_spacing + line_height - 1) / line_height;
        /* Reduce by one line as  */
        if (max_lines > 1) {
            max_lines--;
        }
        if (max_lines <= 0) {
            max_lines = 1;
        }
    } else {
        max_lines = 1;
    }


    int current = start_offset;
    int lines_used = 0;
    int line_start = start_offset;
    int last_space = -1;
    int32_t line_width = 0;

    while (current < (int)reader->current_text_len && lines_used <= max_lines) {
        if (reader->current_text[current] == '\n') {
            lines_used++;
            /* Check if we've exceeded max lines after the newline */
            if (lines_used == max_lines) {
                /* We've exceeded max lines - return position after previous line */
                return current;
            }
            line_start = current + 1;
            line_width = 0;
            last_space = -1;
            current++;
            continue;
        }

        if (reader->current_text[current] == '\0') {
            break;
        }

        uint32_t unicode = (unsigned char)reader->current_text[current];
        uint32_t next_unicode = (current + 1 < (int)reader->current_text_len && reader->current_text[current + 1] != '\0')
                                ? (unsigned char)reader->current_text[current + 1] : 0;
        lv_font_glyph_dsc_t g_dsc;
        if (font->get_glyph_dsc && font->get_glyph_dsc(font, &g_dsc, unicode, next_unicode)) {
            int char_width = g_dsc.adv_w;
            if (current < (int)reader->current_text_len - 1 && !isspace((unsigned char)reader->current_text[current])) {
                char_width += letter_spacing;
            }

            if (isspace((unsigned char)reader->current_text[current])) {
                last_space = current;
            }

            /* Check if adding this character would exceed line width */
            /* Use >= to prevent any overflow - be conservative */
            if (line_width + char_width >= reader->display.text_area_width && line_width > 0) {
                /* Check if we can fit another line before wrapping */
                if (lines_used >= max_lines) {
                    /* We've reached max lines - find last word boundary */
                    /* Always break at word boundary, never split words */
                    if (last_space >= line_start) {
                        /* Return after the last space in current line */
                        return last_space + 1;
                    }
                    /* No space found in current line - return start of current line */
                    /* This prevents word splitting */
                    return line_start;
                }

                /* Need to wrap to next line */
                if (last_space >= line_start) {
                    /* Wrap at last space */
                    current = last_space + 1;
                    line_start = last_space + 1;
                } else {
                    /* No space found, wrap here (word will be split on this line) */
                    line_start = current;
                }
                lines_used++;
                line_width = 0;
                last_space = -1;
            } else {
                line_width += char_width;
                current++;
            }
        } else {
            current++;
        }
    }

    /* If we've used all lines, find the last word boundary */
    if (lines_used >= max_lines) {
        /* We've reached max lines - find last word boundary to avoid word splitting */
        /* Use last_space if available, otherwise search backwards */
        if (last_space >= line_start) {
            /* Return after the last space we found */
            return last_space + 1;
        }
        /* Search backwards from current position to find the last space */
        int search_pos = current;
        while (search_pos > line_start && search_pos > start_offset) {
            if (isspace((unsigned char)reader->current_text[search_pos - 1])) {
                /* Found a space - return after it */
                return search_pos;
            }
            search_pos--;
        }
        /* No space found - return start of current line to avoid word splitting */
        return line_start;
    }

    /* Return current position - end of text or available space */
    return current;
}

/* Find previous page start using screen-based calculation */
static int find_prev_page_start(ui_ebook_reader_t *reader, int start_offset)
{
    if (!reader || !reader->current_text || !reader->display.font) {
        return 0;
    }

    if (start_offset <= 0) {
        return 0;
    }

    calculate_text_area(reader);
    if (reader->display.text_area_width <= 0 || reader->display.text_area_height <= 0) {
        return 0;
    }

    const lv_font_t *font = reader->display.font;
    int32_t letter_spacing = reader->display.letter_spacing;
    int32_t line_height = reader->display.line_height;
    int32_t line_spacing = reader->display.line_spacing_px;

    /* Calculate maximum lines that can fit
     * For N lines, total height = N * font->line_height + (N-1) * line_spacing
     * Solving for N: N = (text_area_height + line_spacing) / line_height
     * The last line doesn't need spacing below it, so we account for that */
    int max_lines;
    if (line_height > 0) {
        /* Account for the fact that last line doesn't need spacing below */
        max_lines = (reader->display.text_area_height + line_spacing) / line_height;
        if (max_lines <= 0) {
            max_lines = 1;
        }
        /* Don't subtract safety margin - use full available height */
        /* The formula already accounts for proper spacing */
    } else {
        max_lines = 1;
    }

    /* Work backwards from start_offset */
    int lines_counted = 0;
    int current = start_offset - 1;
    int line_end = start_offset;
    int last_space = -1;
    int32_t line_width = 0;

    /* First, find where the previous page would end (one page before start_offset) */
    while (current >= 0 && lines_counted < max_lines) {
        if (reader->current_text[current] == '\n') {
            lines_counted++;
            if (lines_counted >= max_lines) {
                return current + 1;
            }
            line_end = current;
            line_width = 0;
            last_space = -1;
            current--;
            continue;
        }

        if (reader->current_text[current] == '\0') {
            break;
        }

        uint32_t unicode = (unsigned char)reader->current_text[current];
        uint32_t next_unicode = (current + 1 < (int)reader->current_text_len && reader->current_text[current + 1] != '\0')
                                ? (unsigned char)reader->current_text[current + 1] : 0;
        lv_font_glyph_dsc_t g_dsc;
        if (font->get_glyph_dsc && font->get_glyph_dsc(font, &g_dsc, unicode, next_unicode)) {
            int char_width = g_dsc.adv_w;
            if (current > 0 && !isspace((unsigned char)reader->current_text[current - 1])) {
                char_width += letter_spacing;
            }

            if (isspace((unsigned char)reader->current_text[current])) {
                last_space = current;
            }

            /* Check if adding this character would exceed line width */
            int prev_char_start = current;
            while (prev_char_start > 0 && !isspace((unsigned char)reader->current_text[prev_char_start - 1]) &&
                   reader->current_text[prev_char_start - 1] != '\n') {
                prev_char_start--;
            }

            int32_t test_line_width = calculate_text_width(reader->current_text + prev_char_start,
                                                           line_end - prev_char_start, font, letter_spacing);

            if (test_line_width > reader->display.text_area_width && line_width > 0) {
                /* This would cause a wrap, so previous line ends here */
                lines_counted++;
                if (lines_counted >= max_lines) {
                    if (last_space >= prev_char_start) {
                        return last_space + 1;
                    }
                    return prev_char_start;
                }
                line_end = prev_char_start;
                line_width = 0;
                last_space = -1;
            }
            current--;
        } else {
            current--;
        }
    }

    /* If we've counted enough lines, return the start */
    if (lines_counted >= max_lines) {
        if (last_space >= 0) {
            return last_space + 1;
        }
        return (current >= 0) ? current + 1 : 0;
    }

    return 0;
}

/* Free page cache */
static void free_page_cache(ui_ebook_reader_t *reader)
{
    if (!reader) {
        return;
    }

    if (reader->page_cache) {
        for (int i = 0; i < reader->spine_count; i++) {
            if (reader->page_cache[i]) {
                lv_free(reader->page_cache[i]);
            }
        }
        lv_free(reader->page_cache);
        reader->page_cache = NULL;
    }

    if (reader->page_count_per_chapter) {
        lv_free(reader->page_count_per_chapter);
        reader->page_count_per_chapter = NULL;
    }

    reader->page_cache_valid = false;
}

/* Generate cache filename for page cache */
static char *get_page_cache_filename(const char *filepath)
{
    static char cache_file[512];
    char *cache_dir = CACHE_DIR;

    /* Extract filename from path */
    const char *filename = strrchr(filepath, '/');
    if (!filename) {
        filename = strrchr(filepath, '\\');
    }
    if (!filename) {
        filename = filepath;
    } else {
        filename++;
    }

    /* Sanitize filename */
    char sanitized[256];
    int j = 0;
    for (int i = 0; filename[i] && j < (int)(sizeof(sanitized) - 1); i++) {
        char c = filename[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') {
            sanitized[j++] = c;
        } else {
            sanitized[j++] = '_';
        }
    }
    sanitized[j] = '\0';

    snprintf(cache_file, sizeof(cache_file), "%s/%s.page_cache", cache_dir, sanitized);
    return cache_file;
}

/* Save page cache to file */
static bool save_page_cache(ui_ebook_reader_t *reader, const char *filepath)
{
    if (!reader || !reader->page_cache_valid || !filepath) {
        return false;
    }

    char *cache_file = get_page_cache_filename(filepath);

    /* Ensure cache directory exists */
    struct stat st;
    char *cache_dir = CACHE_DIR;
    if (stat(cache_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        /* Create directory */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", cache_dir);
        system(cmd);
    }

    FILE *f = fopen(cache_file, "w");
    if (!f) {
        return false;
    }

    /* Write header: font_line_height (determines font size), line_spacing, letter_spacing, text_area_width, text_area_height */
    fprintf(f, "PAGE_CACHE_V1\n");
    int32_t font_line_height = reader->display.font ? reader->display.font->line_height : 0;
    fprintf(f, "SETTINGS:%d:%d:%d:%d:%d\n", font_line_height, reader->display.line_spacing,
            reader->display.letter_spacing, reader->display.text_area_width, reader->display.text_area_height);
    int max_chapters = reader->is_epub ? reader->spine_count : 1;
    fprintf(f, "CHAPTERS:%d\n", max_chapters);

    /* Write page boundaries for each chapter */
    for (int ch = 0; ch < max_chapters; ch++) {
        if (reader->page_count_per_chapter && reader->page_cache && reader->page_cache[ch]) {
            fprintf(f, "CHAPTER:%d:%d:", ch, reader->page_count_per_chapter[ch]);
            for (int pg = 0; pg < reader->page_count_per_chapter[ch]; pg++) {
                fprintf(f, "%d", reader->page_cache[ch][pg]);
                if (pg < reader->page_count_per_chapter[ch] - 1) {
                    fprintf(f, ",");
                }
            }
            fprintf(f, "\n");
        }
    }

    fclose(f);
    return true;
}

/* Load page cache from file */
static bool load_page_cache(ui_ebook_reader_t *reader, const char *filepath)
{
    if (!reader || !filepath) {
        return false;
    }

    char *cache_file = get_page_cache_filename(filepath);
    FILE *f = fopen(cache_file, "r");
    if (!f) {
        return false;
    }

    char line[2048];
    // int version = 0;  // Reserved for future version checking
    int32_t cached_font_line_height = 0;  /* Font size is determined by font->line_height */
    int cached_line_spacing = UI_EBOOK_LINE_SPACING_TIGHT;
    int cached_letter_spacing = 0;
    int32_t cached_width = 0;
    int32_t cached_height = 0;
    int chapters = 0;

    /* Read header */
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    if (strncmp(line, "PAGE_CACHE_V1", 13) != 0) {
        fclose(f);
        return false;
    }

    /* Read settings - font_line_height determines font size */
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    if (sscanf(line, "SETTINGS:%d:%d:%d:%d:%d", &cached_font_line_height, &cached_line_spacing,
               &cached_letter_spacing, &cached_width, &cached_height) != 5) {
        fclose(f);
        return false;
    }

    /* Check if settings match current settings */
    calculate_text_area(reader);
    int32_t current_font_line_height = reader->display.font ? reader->display.font->line_height : 0;
    if (cached_font_line_height != current_font_line_height ||
        cached_line_spacing != reader->display.line_spacing ||
        cached_letter_spacing != reader->display.letter_spacing ||
        cached_width != reader->display.text_area_width ||
        cached_height != reader->display.text_area_height) {
        fclose(f);
        return false; /* Settings changed, cache invalid */
    }

    /* Read chapters count */
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    if (sscanf(line, "CHAPTERS:%d", &chapters) != 1 || chapters != reader->spine_count) {
        fclose(f);
        return false;
    }

    /* Allocate page cache */
    reader->page_cache = (int **)lv_malloc(sizeof(int *) * reader->spine_count);
    reader->page_count_per_chapter = (int *)lv_malloc(sizeof(int) * reader->spine_count);
    if (!reader->page_cache || !reader->page_count_per_chapter) {
        fclose(f);
        free_page_cache(reader);
        return false;
    }

    memset(reader->page_cache, 0, sizeof(int *) * reader->spine_count);
    memset(reader->page_count_per_chapter, 0, sizeof(int) * reader->spine_count);

    /* Read chapter data */
    for (int ch = 0; ch < reader->spine_count; ch++) {
        if (!fgets(line, sizeof(line), f)) {
            break;
        }

        int chapter_idx, page_count;
        if (sscanf(line, "CHAPTER:%d:%d:", &chapter_idx, &page_count) != 2) {
            continue;
        }

        if (chapter_idx != ch || page_count <= 0) {
            continue;
        }

        reader->page_count_per_chapter[ch] = page_count;
        reader->page_cache[ch] = (int *)lv_malloc(sizeof(int) * page_count);
        if (!reader->page_cache[ch]) {
            continue;
        }

        /* Parse page offsets */
        char *offset_str = strchr(line, ':');
        if (offset_str) {
            offset_str = strchr(offset_str + 1, ':');
            if (offset_str) {
                offset_str = strchr(offset_str + 1, ':');
                if (offset_str) {
                    offset_str++; /* Skip the colon */
                    char *token = strtok(offset_str, ",");
                    int pg = 0;
                    while (token && pg < page_count) {
                        reader->page_cache[ch][pg] = atoi(token);
                        token = strtok(NULL, ",");
                        pg++;
                    }
                }
            }
        }
    }

    fclose(f);
    reader->page_cache_valid = true;
    return true;
}

/* Build page cache for all chapters */
static void build_page_cache(ui_ebook_reader_t *reader)
{
    if (!reader) {
        return;
    }

    /* Free existing cache */
    free_page_cache(reader);

    /* Calculate text area first - ensure layout is updated */
    lv_obj_update_layout(reader->text_label);
    calculate_text_area(reader);
    if (reader->display.text_area_width <= 0 || reader->display.text_area_height <= 0) {
        LV_LOG_WARN("build_page_cache: Invalid text area dimensions %dx%d",
                    reader->display.text_area_width, reader->display.text_area_height);
        return;
    }

    /* Allocate page cache arrays */
    int max_chapters = reader->is_epub ? reader->spine_count : 1;
    reader->page_cache = (int **)lv_malloc(sizeof(int *) * max_chapters);
    reader->page_count_per_chapter = (int *)lv_malloc(sizeof(int) * max_chapters);
    if (!reader->page_cache || !reader->page_count_per_chapter) {
        return;
    }

    memset(reader->page_cache, 0, sizeof(int *) * max_chapters);
    memset(reader->page_count_per_chapter, 0, sizeof(int) * max_chapters);

    /* Store current chapter and offset */
    int saved_chapter = reader->current_chapter;
    int saved_offset = reader->current_offset;

    /* Build cache for each chapter (or single TXT file) */
    for (int ch = 0; ch < max_chapters; ch++) {
        if (!load_chapter(reader, ch)) {
            continue;
        }

        if (!reader->current_text || reader->current_text_len == 0) {
            continue;
        }

        /* Calculate pages for this chapter */
        int *page_offsets = (int *)lv_malloc(sizeof(int) * 1000); /* Start with 1000 pages max */
        if (!page_offsets) {
            continue;
        }

        int page_count = 0;
        int offset = 0;
        page_offsets[page_count++] = 0; /* First page starts at 0 */

        while (offset < (int)reader->current_text_len) {
            int next_offset = find_next_page_end(reader, offset);
            if (next_offset <= offset || next_offset >= (int)reader->current_text_len) {
                break;
            }
            offset = next_offset;
            page_offsets[page_count++] = offset;

            /* Reallocate if needed */
            if (page_count >= 1000) {
                int *new_offsets = (int *)lv_realloc(page_offsets, sizeof(int) * (page_count + 1000));
                if (new_offsets) {
                    page_offsets = new_offsets;
                } else {
                    break;
                }
            }
        }

        reader->page_cache[ch] = page_offsets;
        reader->page_count_per_chapter[ch] = page_count;
    }

    /* Restore saved chapter - reload it to ensure text is correct */
    if (saved_chapter >= 0 && saved_chapter < max_chapters) {
        load_chapter(reader, saved_chapter);
        reader->current_offset = saved_offset;
        /* Ensure offset is within bounds */
        if (reader->current_offset > (int)reader->current_text_len) {
            reader->current_offset = 0;
        }
    } else {
        /* Load first chapter if saved chapter is invalid */
        if (max_chapters > 0) {
            load_chapter(reader, 0);
            reader->current_offset = 0;
        }
    }

    reader->page_cache_valid = true;
}

/* Update text label style with current settings */
/* NOTE: This function rebuilds the page cache when font, line spacing, or letter spacing changes.
 * Margin/padding changes would affect text_area_width/height, which invalidates the cache
 * and triggers a rebuild on next access. */
static void update_text_style(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->text_label) {
        return;
    }

    /* Set font */
    if (reader->display.font) {
        lv_obj_set_style_text_font(reader->text_label, reader->display.font, LV_PART_MAIN);
    }

    /* Set letter spacing */
    lv_obj_set_style_text_letter_space(reader->text_label, reader->display.letter_spacing, LV_PART_MAIN);

    /* Set line spacing - use calculated values from display settings */
    /* update_display_calculations() already calculated line_spacing_px */
    if (reader->display.font) {
        lv_obj_set_style_text_line_space(reader->text_label, reader->display.line_spacing_px, LV_PART_MAIN);
    }

    /* Recalculate text area after style change */
    /* This includes padding/margin effects on available text space */
    calculate_text_area(reader);

    /* Rebuild page cache when font, line spacing, or letter spacing changes */
    /* This ensures the entire EPUB is re-indexed and page splits are recalculated */
    /* NOTE: Text size (font size) is determined by font files and cannot be changed */
    /* NOTE: Margin/padding changes affect text_area_width/height, which invalidates cache */
    /* Only rebuild if cache was already built (i.e., not during initial setup) */
    if (reader->page_cache_valid) {
        build_page_cache(reader);

        /* Save the updated cache */
        extern const char *ui_file_screen_get_current_filepath(void);
        const char *filepath = ui_file_screen_get_current_filepath();
        if (filepath) {
            save_page_cache(reader, filepath);
        }

        /* Update display to show current page with new settings */
        ui_ebook_reader_display_page(reader);
    }
}

/* Read TXT file content */
static char *read_txt_file(const char *filepath)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = (char *)lv_malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buffer, 1, file_size, fp);
    buffer[read] = '\0';
    fclose(fp);

    return buffer;
}

/* Load chapter text */
static bool load_chapter(ui_ebook_reader_t *reader, int chapter_idx)
{
    if (reader->is_epub) {
        if (chapter_idx < 0 || chapter_idx >= reader->spine_count) {
            PR_ERR("[LOAD_CHAPTER] Invalid chapter index: %d (max: %d)", chapter_idx, reader->spine_count);
            return false;
        }

        epub_content_item_t *item = reader->spine_items[chapter_idx];
        if (!item || !item->href) {
            PR_ERR("[LOAD_CHAPTER] Invalid spine item at index %d: item=%p, href=%p", 
                   chapter_idx, item, item ? item->href : NULL);
            return false;
        }
        
        if (!reader->reader) {
            PR_ERR("[LOAD_CHAPTER] reader->reader is NULL!");
            return false;
        }

        PR_DEBUG("[LOAD_CHAPTER] Loading chapter %d, href=%s", chapter_idx, item->href);

        if (reader->current_text) {
            /* epub_get_text_content uses malloc(), so use free() */
            free(reader->current_text);
            reader->current_text = NULL;
        }

        reader->current_text = epub_get_text_content(reader->reader, item->href);
        if (!reader->current_text) {
            PR_ERR("[LOAD_CHAPTER] epub_get_text_content failed for chapter %d, href=%s", 
                   chapter_idx, item->href);
            return false;
        }
        
        PR_DEBUG("[LOAD_CHAPTER] Successfully loaded chapter %d, text_len=%zu", 
                 chapter_idx, strlen(reader->current_text));

        reader->current_text_len = strlen(reader->current_text);
        reader->current_chapter  = chapter_idx;
        reader->current_offset   = 0;
    } else {
        /* For TXT files, load entire file */
        if (reader->current_text) {
            lv_free(reader->current_text);
            reader->current_text = NULL;
        }

        reader->current_text = read_txt_file(reader->txt_filepath);
        if (!reader->current_text) {
            return false;
        }

        reader->current_text_len = strlen(reader->current_text);
        reader->current_chapter  = 0;
        reader->current_offset   = 0;
    }

    return true;
}

/* Clear all character boxes */
static void clear_character_boxes(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->char_boxes_container) {
        return;
    }
    /* Delete all children (character boxes) */
    uint32_t child_cnt = lv_obj_get_child_cnt(reader->char_boxes_container);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(reader->char_boxes_container, 0);
        if (child) {
            lv_obj_del(child);
        }
    }
}

/* Draw green boxes around all characters in the displayed text */
__attribute__((unused)) static void draw_character_boxes(ui_ebook_reader_t *reader, const char *text, int text_len)
{
    if (!reader || !text || text_len <= 0 || !reader->display.font) {
        return;
    }

    /* Clear existing boxes */
    clear_character_boxes(reader);

    /* Create container if it doesn't exist */
    if (!reader->char_boxes_container) {
        lv_obj_t *parent = lv_obj_get_parent(reader->text_label);
        reader->char_boxes_container = lv_obj_create(parent);
        lv_obj_set_size(reader->char_boxes_container, LV_PCT(100), LV_PCT(100));
        lv_obj_align(reader->char_boxes_container, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_opa(reader->char_boxes_container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(reader->char_boxes_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(reader->char_boxes_container, 0, LV_PART_MAIN);
        lv_obj_clear_flag(reader->char_boxes_container, LV_OBJ_FLAG_CLICKABLE);
        /* Move behind text label */
        lv_obj_move_background(reader->char_boxes_container);
    }

    /* Get text label position and size */
    lv_obj_update_layout(reader->text_label);
    int32_t label_x = lv_obj_get_x(reader->text_label);
    int32_t label_y = lv_obj_get_y(reader->text_label);
    int32_t label_pad_left = lv_obj_get_style_pad_left(reader->text_label, LV_PART_MAIN);
    int32_t label_pad_top = lv_obj_get_style_pad_top(reader->text_label, LV_PART_MAIN);

    calculate_text_area(reader);
    const lv_font_t *font = reader->display.font;
    int letter_spacing = reader->display.letter_spacing;

    /* Calculate line height */
    float line_height_mult = 1.0f;
    switch (reader->display.line_spacing) {
        case UI_EBOOK_LINE_SPACING_TIGHT: line_height_mult = 1.0f; break;
        case UI_EBOOK_LINE_SPACING_MID: line_height_mult = 1.2f; break;
        case UI_EBOOK_LINE_SPACING_LOOSE: line_height_mult = 1.5f; break;
    }
    int32_t line_height = (int32_t)(font->line_height * line_height_mult);

    /* Draw boxes for each character */
    int32_t x = label_x + label_pad_left;
    int32_t y = label_y + label_pad_top;
    int32_t line_start_x = x;
    int32_t line_width = 0;

    for (int i = 0; i < text_len && text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            /* New line */
            y += line_height;
            x = line_start_x;
            line_width = 0;
            continue;
        }

        /* Get character metrics */
        uint32_t unicode = (unsigned char)text[i];
        uint32_t next_unicode = (i + 1 < text_len && text[i + 1] != '\0') ? (unsigned char)text[i + 1] : 0;
        lv_font_glyph_dsc_t g_dsc;
        int32_t char_advance = 0;
        int32_t char_box_w = 0;
        int32_t char_box_h = 0;
        int32_t char_ofs_x = 0;
        int32_t char_ofs_y = 0;

        if (font->get_glyph_dsc && font->get_glyph_dsc(font, &g_dsc, unicode, next_unicode)) {
            char_advance = g_dsc.adv_w;
            char_box_w = g_dsc.box_w;
            char_box_h = g_dsc.box_h;
            char_ofs_x = g_dsc.ofs_x;
            char_ofs_y = g_dsc.ofs_y;

            /* Use box width if available, otherwise use advance width */
            if (char_box_w <= 0) {
                char_box_w = char_advance;
            }
            /* Use box height if available, otherwise use line height */
            if (char_box_h <= 0) {
                char_box_h = line_height;
            }

            /* Add letter spacing for non-space characters */
            if (i < text_len - 1 && !isspace((unsigned char)text[i])) {
                char_advance += letter_spacing;
            }
        } else {
            /* Fallback for unsupported characters */
            char_advance = font->line_height / 2;
            char_box_w = char_advance;
            char_box_h = line_height;
        }

        /* Check if we need to wrap (use advance for wrapping calculation) */
        if (line_width + char_advance > reader->display.text_area_width && line_width > 0) {
            /* Wrap to next line */
            y += line_height;
            x = line_start_x;
            line_width = 0;
            /* Recalculate char metrics at start of new line */
            if (font->get_glyph_dsc && font->get_glyph_dsc(font, &g_dsc, unicode, next_unicode)) {
                char_advance = g_dsc.adv_w;
                char_box_w = g_dsc.box_w;
                char_box_h = g_dsc.box_h;
                char_ofs_x = g_dsc.ofs_x;
                char_ofs_y = g_dsc.ofs_y;
                if (char_box_w <= 0) char_box_w = char_advance;
                if (char_box_h <= 0) char_box_h = line_height;
                if (i < text_len - 1 && !isspace((unsigned char)text[i])) {
                    char_advance += letter_spacing;
                }
            }
        }

        /* Create green box for this character */
        lv_obj_t *box = lv_obj_create(reader->char_boxes_container);
        /* Use advance width for box width (this is what determines character spacing) */
        /* But ensure we account for the full glyph extent including offsets */
        int32_t box_width = char_advance;
        /* For box height, use line height to cover the full character area */
        int32_t box_height = line_height;

        /* If glyph has a larger box, use that but ensure minimum is advance width */
        if (char_box_w > 0 && char_box_w > char_advance) {
            box_width = char_box_w;
        }
        /* Account for negative offsets that extend beyond the base position */
        if (char_ofs_x < 0) {
            box_width += (-char_ofs_x);
        }
        if (char_box_w > 0 && (char_ofs_x + char_box_w) > char_advance) {
            box_width = char_ofs_x + char_box_w;
        }

        lv_obj_set_size(box, box_width, box_height);
        /* Position box - account for glyph offset, but ensure it starts at or before x */
        int32_t box_x = x;
        if (char_ofs_x < 0) {
            box_x = x + char_ofs_x; /* Negative offset means glyph extends left */
        }
        int32_t box_y = y + char_ofs_y; /* Vertical offset from baseline */
        /* Ensure box_y doesn't go negative */
        if (box_y < y) {
            box_height += (y - box_y);
            box_y = y;
        }
        lv_obj_set_pos(box, box_x, box_y);
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(box, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(box, 0, LV_PART_MAIN);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        /* Move cursor by advance width (not box width) */
        x += char_advance;
        line_width += char_advance;
    }
}

/* Initialize ebook reader */
ui_ebook_reader_t *ui_ebook_reader_init(const char *filepath, lv_obj_t *container)
{
    PR_DEBUG("[EBOOK_INIT] Start: filepath=%s, container=%p", filepath, container);
    
    if (!filepath || !container) {
        PR_ERR("[EBOOK_INIT] Invalid parameters: filepath=%p, container=%p", filepath, container);
        return NULL;
    }

    ui_ebook_reader_t *reader = (ui_ebook_reader_t *)lv_malloc(sizeof(ui_ebook_reader_t));
    if (!reader) {
        PR_ERR("[EBOOK_INIT] Failed to allocate reader memory");
        return NULL;
    }

    PR_DEBUG("[EBOOK_INIT] Reader allocated: %p, size=%zu", reader, sizeof(ui_ebook_reader_t));
    memset(reader, 0, sizeof(ui_ebook_reader_t));

    /* Check file extension */
    size_t path_len = strlen(filepath);
    reader->is_epub = (path_len > 5 && strcmp(filepath + path_len - 5, ".epub") == 0);
    
    PR_DEBUG("[EBOOK_INIT] File extension check: is_epub=%d, path_len=%zu", reader->is_epub, path_len);

    if (reader->is_epub) {
        /* Open EPUB file */
        PR_DEBUG("[EBOOK_INIT] Opening EPUB file...");
        reader->reader = epub_open(filepath);
        
        PR_DEBUG("[EBOOK_INIT] epub_open returned: %p", reader->reader);
        
        if (!reader->reader) {
            PR_ERR("[EBOOK_INIT] epub_open failed");
            lv_free(reader);
            return NULL;
        }

        /* Get configuration */
        reader->config = epub_get_default_config();
        epub_set_config(reader->reader, &reader->config);

        /* Get spine items */
        reader->spine_items = epub_get_spine_items(reader->reader, &reader->spine_count);
        if (!reader->spine_items || reader->spine_count == 0) {
            epub_close(reader->reader);
            lv_free(reader);
            return NULL;
        }

        /* Get TOC */
        reader->toc = epub_get_toc(reader->reader);

        /* Restore progress */
        int saved_chapter = 0, saved_page = 0, saved_offset = 0;
        epub_get_progress_status(reader->reader, &saved_chapter, &saved_page, &saved_offset);

        /* Load first valid chapter or saved chapter */
        int first_chapter = (saved_chapter >= 0 && saved_chapter < reader->spine_count) ? saved_chapter : 0;
        bool loaded = false;
        while (first_chapter < reader->spine_count && !loaded) {
            if (load_chapter(reader, first_chapter)) {
                loaded = true;
                if (saved_offset > 0 && saved_offset <= (int)reader->current_text_len) {
                    reader->current_offset = saved_offset;
                }
            } else {
                first_chapter++;
            }
        }

        if (!loaded) {
            epub_close(reader->reader);
            lv_free(reader);
            return NULL;
        }
    } else {
        /* For TXT files */
        reader->txt_filepath = lv_malloc(strlen(filepath) + 1);
        if (!reader->txt_filepath) {
            lv_free(reader);
            return NULL;
        }
        strcpy(reader->txt_filepath, filepath);

        reader->config = epub_get_default_config();
        reader->spine_count = 1;

        if (!load_chapter(reader, 0)) {
            lv_free(reader->txt_filepath);
            lv_free(reader);
            return NULL;
        }
    }

    /* Initialize display settings with defaults (includes margins) */
    init_display_settings(&reader->display);

    /* Load font dynamically from binary file */
    /* Verify filesystem is ready */
    if (!lv_fs_is_ready('A')) {
        LV_LOG_ERROR("Filesystem driver 'A' is not ready!");
    }
    LV_LOG_USER("Loading font from %s (filesystem ready: %s)", reader->display.font_path, lv_fs_is_ready('A') ? "yes" : "no");
    reader->display.font = lv_binfont_create(reader->display.font_path);
    if (!reader->display.font) {
        LV_LOG_ERROR("Failed to load font from %s (possibly out of memory or filesystem error), using fallback", reader->display.font_path);
        /* Fallback to default font if loading fails - cast away const for compatibility */
        reader->display.font = (lv_font_t *)&ui_font_JetBrains_Mono_Regular_16_2;
        LV_LOG_WARN("Using fallback font with line_height: %d", reader->display.font->line_height);
    } else {
        LV_LOG_USER("Successfully loaded font from %s", reader->display.font_path);
        LV_LOG_USER("Loaded font line_height: %d pixels (expected 14px)", reader->display.font->line_height);
        if (reader->display.font->line_height != 14) {
            LV_LOG_WARN("WARNING: Font line_height is %d, but expected 14px! Check font binary file.", reader->display.font->line_height);
        }
    }

    /* Font size is determined by the loaded font binary, not a setting */
    /* Line spacing and margins are already set by init_display_settings() */
    /* Update calculations after font is loaded */
    PR_DEBUG("[EBOOK_INIT] Updating display calculations...");
    update_display_calculations(&reader->display);

    reader->display.text_area_width = 0;
    reader->display.text_area_height = 0;
    reader->total_pages = 0;

    /* Initialize page cache */
    reader->page_cache = NULL;
    reader->page_count_per_chapter = NULL;
    reader->page_cache_valid = false;

    PR_DEBUG("[EBOOK_INIT] About to create LVGL widgets - acquiring lock...");
    
    /* 🔒 CRITICAL: Acquire LVGL lock before creating any LVGL objects */
    lv_vendor_disp_lock();
    PR_DEBUG("[EBOOK_INIT] LVGL lock acquired, creating widgets...");
    
    /* Create text label with improved styling */
    reader->text_label = lv_label_create(container);
    PR_DEBUG("[EBOOK_INIT] Text label created: %p", reader->text_label);
    lv_obj_set_width(reader->text_label, LV_PCT(100));
    lv_obj_set_height(reader->text_label, LV_PCT(100));
    lv_obj_align(reader->text_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_long_mode(reader->text_label, LV_LABEL_LONG_WRAP);
    /* Apply margins from display settings */
    lv_obj_set_style_pad_left(reader->text_label, reader->display.margin_left, LV_PART_MAIN);
    lv_obj_set_style_pad_right(reader->text_label, reader->display.margin_right, LV_PART_MAIN);
    lv_obj_set_style_pad_top(reader->text_label, reader->display.margin_top, LV_PART_MAIN);
    /* Set bottom padding to make room for progress bar */
    lv_obj_set_style_pad_bottom(reader->text_label, reader->display.margin_bottom, LV_PART_MAIN);
    /* Pink background for active text container */
    // lv_obj_set_style_bg_color(reader->text_label, lv_color_hex(0xFFC0CB), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(reader->text_label, LV_OPA_COVER, LV_PART_MAIN);
    /* Text color for better readability */
    lv_obj_set_style_text_color(reader->text_label, lv_color_hex(0x212121), LV_PART_MAIN);
    /* Make text label non-clickable so it doesn't intercept keyboard events */
    lv_obj_clear_flag(reader->text_label, LV_OBJ_FLAG_CLICKABLE);

    /* Apply initial style */
    update_text_style(reader);

    /* Force layout update and recalculate text area after styles are applied */
    lv_obj_update_layout(reader->text_label);
    calculate_text_area(reader);

    /* Create progress line with improved styling */
    reader->progress_line = lv_line_create(container);
    lv_obj_set_width(reader->progress_line, LV_PCT(100));
    lv_obj_set_height(reader->progress_line, 3); /* 2px height for progress bar */
    lv_obj_align(reader->progress_line, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    /* Blue background for progress bar container */
    // lv_obj_set_style_bg_color(reader->progress_line, lv_color_hex(0x0000FF), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(reader->progress_line, LV_OPA_COVER, LV_PART_MAIN);
    /* Progress bar styling */
    lv_obj_set_style_line_width(reader->progress_line, 3, LV_PART_MAIN);
    lv_obj_set_style_line_opa(reader->progress_line, LV_OPA_COVER, LV_PART_MAIN);
    /* Add rounded line caps */
    lv_obj_set_style_line_rounded(reader->progress_line, true, LV_PART_MAIN);
    lv_obj_clear_flag(reader->progress_line, LV_OBJ_FLAG_CLICKABLE);

    /* Recalculate text area now that progress line is created */
    calculate_text_area(reader);

    reader->char_boxes_container = NULL;

    /* Initialize settings page */
    reader->settings_overlay = NULL;
    reader->settings_left_panel = NULL;
    reader->settings_right_panel = NULL;
    reader->settings_menu_list = NULL;
    reader->settings_stats_label = NULL;
    reader->settings_selected_idx = 0;

    /* Initialize font selection page */
    reader->font_selection_overlay = NULL;
    reader->font_selection_list = NULL;
    reader->font_selection_selected_idx = 0;
    reader->font_list = NULL;
    reader->font_list_count = 0;

    /* Ensure text area is calculated before building cache */
    /* This must be done after all styles are applied */
    PR_DEBUG("[EBOOK_INIT] Updating layout and calculating text area...");
    lv_obj_update_layout(reader->text_label);
    calculate_text_area(reader);
    
    PR_DEBUG("[EBOOK_INIT] LVGL widget setup complete, releasing lock for cache building...");
    /* 🔓 Release LVGL lock before building cache (file I/O) */
    lv_vendor_disp_unlock();
    PR_DEBUG("[EBOOK_INIT] LVGL lock released");

    /* Try to load page cache */
    PR_DEBUG("[EBOOK_INIT] Loading/building page cache...");
    if (reader->is_epub) {
        if (!load_page_cache(reader, filepath)) {
            /* Cache doesn't exist or is invalid, build it */
            build_page_cache(reader);
            /* Save the cache */
            save_page_cache(reader, filepath);
        }
    } else {
        /* For TXT files, try to load cache first */
        if (!load_page_cache(reader, filepath)) {
            /* Cache doesn't exist or is invalid, build it */
            build_page_cache(reader);
            /* Save the cache for TXT files too */
            save_page_cache(reader, filepath);
        }
    }
    PR_DEBUG("[EBOOK_INIT] Page cache ready");

    /* Display first page - needs LVGL lock */
    PR_DEBUG("[EBOOK_INIT] Acquiring lock for display_page...");
    lv_vendor_disp_lock();
    ui_ebook_reader_display_page(reader);
    lv_vendor_disp_unlock();
    PR_DEBUG("[EBOOK_INIT] display_page completed, lock released");

    PR_DEBUG("[EBOOK_INIT] ✅✅✅ Initialization complete! Returning reader=%p", reader);
    return reader;
}

/* Cleanup ebook reader */
void ui_ebook_reader_cleanup(ui_ebook_reader_t *reader)
{
    if (!reader) {
        return;
    }

    if (reader->current_text) {
        /* epub_get_text_content uses malloc(), so use free() for EPUB files */
        if (reader->is_epub) {
            free(reader->current_text);
        } else {
            /* TXT files use lv_malloc() */
            lv_free(reader->current_text);
        }
    }

    if (reader->is_epub && reader->reader) {
        /* Save progress */
        if (reader->reader) {
            update_epub_config_font_path(reader);
            int current_page = reader->current_offset / reader->config.chars_per_page;
            epub_save_progress(reader->reader, reader->current_chapter, current_page, reader->current_offset);
            save_lvgl_config_to_cache(reader);
        }
        epub_close(reader->reader);
    } else if (reader->txt_filepath) {
        lv_free(reader->txt_filepath);
    }


    if (reader->text_label) {
        lv_obj_del(reader->text_label);
    }

    if (reader->progress_line) {
        lv_obj_del(reader->progress_line);
    }

    /* Cleanup settings page */
    if (reader->settings_overlay) {
        lv_obj_del(reader->settings_overlay);
    }

    /* Cleanup font selection page */
    if (reader->font_selection_overlay) {
        lv_obj_del(reader->font_selection_overlay);
    }

    /* Free font list */
    if (reader->font_list) {
        for (int i = 0; i < reader->font_list_count; i++) {
            if (reader->font_list[i]) {
                lv_free(reader->font_list[i]);
            }
        }
        lv_free(reader->font_list);
    }

    /* Free dynamically loaded font */
    if (reader->display.font && reader->display.font != (lv_font_t *)&ui_font_JetBrains_Mono_Regular_16_2) {
        lv_binfont_destroy(reader->display.font);
        reader->display.font = NULL;
    }

    /* Free page cache */
    free_page_cache(reader);

    lv_free(reader);
}

/* Find current page index in cache */
static int find_current_page_index(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->page_cache_valid || !reader->page_cache ||
        reader->current_chapter < 0 || reader->current_chapter >= reader->spine_count) {
        return 0;
    }

    int *page_offsets = reader->page_cache[reader->current_chapter];
    int page_count = reader->page_count_per_chapter[reader->current_chapter];

    if (!page_offsets || page_count <= 0) {
        return 0;
    }

    /* Binary search for the page containing current_offset */
    int left = 0, right = page_count - 1;
    int page_idx = 0;

    while (left <= right) {
        int mid = (left + right) / 2;
        if (page_offsets[mid] <= reader->current_offset) {
            page_idx = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return page_idx;
}

/* Clear all character labels */
/* Display current page */
void ui_ebook_reader_display_page(ui_ebook_reader_t *reader)
{
    PR_DEBUG("[EBOOK_DISPLAY] Enter: reader=%p", reader);
    
    if (!reader || !reader->current_text) {
        PR_ERR("[EBOOK_DISPLAY] Invalid reader or no current_text!");
        return;
    }

    PR_DEBUG("[EBOOK_DISPLAY] text_label=%p, current_text_len=%d, current_offset=%d",
             reader->text_label, reader->current_text_len, reader->current_offset);

    int start_offset = reader->current_offset;
    int end_offset;

    /* Use cached page boundaries if available */
    if (reader->page_cache_valid && reader->page_cache &&
        reader->current_chapter >= 0 && reader->current_chapter < reader->spine_count) {
        int page_idx = find_current_page_index(reader);
        int *page_offsets = reader->page_cache[reader->current_chapter];
        int page_count = reader->page_count_per_chapter[reader->current_chapter];

        PR_DEBUG("[EBOOK_DISPLAY] Using page cache: page_idx=%d, page_count=%d", page_idx, page_count);

        if (page_offsets && page_idx < page_count) {
            start_offset = page_offsets[page_idx];
            if (page_idx + 1 < page_count) {
                end_offset = page_offsets[page_idx + 1];
            } else {
                end_offset = reader->current_text_len;
            }
        } else {
            /* Fallback to calculation */
            PR_DEBUG("[EBOOK_DISPLAY] Fallback to calculation");
            end_offset = find_next_page_end(reader, start_offset);
        }
    } else {
        /* No cache, calculate on the fly */
        PR_DEBUG("[EBOOK_DISPLAY] No cache, calculating page boundaries");
        end_offset = find_next_page_end(reader, start_offset);
    }

    if (end_offset > (int)reader->current_text_len) {
        end_offset = reader->current_text_len;
    }

    PR_DEBUG("[EBOOK_DISPLAY] Page range: start=%d, end=%d", start_offset, end_offset);

    /* Extract page text */
    int page_len = end_offset - start_offset;
    if (page_len > 0) {
        char *page_text = (char *)lv_malloc(page_len + 1);
        if (page_text) {
            strncpy(page_text, reader->current_text + start_offset, page_len);
            page_text[page_len] = '\0';
            
            PR_DEBUG("[EBOOK_DISPLAY] Setting text (len=%d, first 50 chars): %.50s", 
                     page_len, page_text);
            
            lv_label_set_text(reader->text_label, page_text);
            lv_free(page_text);
            
            PR_DEBUG("[EBOOK_DISPLAY] Text set successfully");
        } else {
            PR_ERR("[EBOOK_DISPLAY] Failed to allocate memory for page_text");
        }
    } else {
        PR_DEBUG("[EBOOK_DISPLAY] End of chapter, showing message");
        lv_label_set_text(reader->text_label, "End of chapter.");
    }

    /* Update progress line */
    int current_page = ui_ebook_reader_get_current_page(reader);
    int total_pages = ui_ebook_reader_get_total_pages(reader);
    PR_DEBUG("[EBOOK_DISPLAY] Progress: page %d of %d", current_page, total_pages);
    
    if (total_pages > 0 && reader->progress_line) {
        int32_t line_width = lv_obj_get_width(reader->progress_line);
        int32_t progress_width = (int32_t)((line_width * current_page) / total_pages);
        if (progress_width < 0) progress_width = 0;
        if (progress_width > line_width) {
            progress_width = line_width;
        }

        /* Create line points: from (0, 1) to (progress_width, 1) */
        static lv_point_precise_t line_points[2];
        line_points[0].x = 0;
        line_points[0].y = 1;
        line_points[1].x = progress_width;
        line_points[1].y = 1;

        lv_line_set_points(reader->progress_line, line_points, 2);
        PR_DEBUG("[EBOOK_DISPLAY] Progress line updated");
    }
    
    PR_DEBUG("[EBOOK_DISPLAY] Exit");
}

/* Navigate to next page */
bool ui_ebook_reader_next_page(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->current_text) {
        return false;
    }

    /* Debug output: Print font size and character grid dimensions */
    calculate_text_area(reader);
    if (reader->display.font && reader->display.text_area_width > 0 && reader->display.text_area_height > 0) {
        /* Calculate character grid */
        int32_t char_width_avg = reader->display.font->line_height / 2; /* Approximate average char width */
        if (char_width_avg <= 0) char_width_avg = 8; /* Fallback */

        /* Use calculated values from display settings */
        // int32_t line_height = reader->display.line_height; /* Already calculated with line spacing multiplier */
        int32_t line_height = 14;

        /* Calculate chars per line: account for letter spacing between characters */
        int chars_per_line = reader->display.text_area_width / (char_width_avg + reader->display.letter_spacing);
        /* Calculate lines per page: account for line spacing (last line doesn't need spacing below) */
        int lines_per_page = (reader->display.text_area_height + reader->display.line_spacing_px) / line_height;

        if (chars_per_line <= 0) chars_per_line = 1;
        if (lines_per_page <= 0) lines_per_page = 1;

        LV_LOG_USER("=== NEXT PAGE DEBUG ===");
        LV_LOG_USER("Font path: %s", reader->display.font_path);
        LV_LOG_USER("Font line_height: %d pixels (should be 14 for 14px font)", reader->display.font->line_height);
        LV_LOG_USER("Text area: %dx%d pixels", reader->display.text_area_width, reader->display.text_area_height);
        LV_LOG_USER("Character grid: %d x %d chars (cols x rows)", chars_per_line, lines_per_page);
        LV_LOG_USER("Total chars per page: ~%d", chars_per_line * lines_per_page);
        LV_LOG_USER("Letter spacing: %dpx", reader->display.letter_spacing);
        LV_LOG_USER("Line spacing multiplier: %.2f", reader->display.line_height_mult);
        LV_LOG_USER("Calculated line_height: %d pixels", reader->display.line_height);
        LV_LOG_USER("========================");
    }

    /* Use cached page boundaries if available */
    if (reader->page_cache_valid && reader->page_cache &&
        reader->current_chapter >= 0 && reader->current_chapter < reader->spine_count) {
        int page_idx = find_current_page_index(reader);
        int *page_offsets = reader->page_cache[reader->current_chapter];
        int page_count = reader->page_count_per_chapter[reader->current_chapter];

        if (page_offsets && page_idx + 1 < page_count) {
            /* Move to next page in same chapter */
            reader->current_offset = page_offsets[page_idx + 1];
            ui_ebook_reader_display_page(reader);

            /* Save progress */
            if (reader->is_epub && reader->reader) {
                update_epub_config_font_path(reader);
                epub_save_progress(reader->reader, reader->current_chapter, page_idx + 1, reader->current_offset);
                save_lvgl_config_to_cache(reader);
            }
            return true;
        } else {
            /* End of chapter, try next chapter */
            if (reader->is_epub) {
                return ui_ebook_reader_next_chapter(reader);
            }
            return false;
        }
    } else {
        /* No cache, calculate on the fly */
        int new_offset = find_next_page_end(reader, reader->current_offset);
        if (new_offset >= (int)reader->current_text_len) {
            if (reader->is_epub) {
                return ui_ebook_reader_next_chapter(reader);
            }
            return false;
        }
        reader->current_offset = new_offset;
        ui_ebook_reader_display_page(reader);

        /* Save progress */
        if (reader->is_epub && reader->reader) {
            update_epub_config_font_path(reader);
            int page_idx = find_current_page_index(reader);
            epub_save_progress(reader->reader, reader->current_chapter, page_idx, reader->current_offset);
            save_lvgl_config_to_cache(reader);
        }
        return true;
    }
}

/* Navigate to previous page */
bool ui_ebook_reader_prev_page(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->current_text) {
        return false;
    }

    /* Use cached page boundaries if available */
    if (reader->page_cache_valid && reader->page_cache &&
        reader->current_chapter >= 0 && reader->current_chapter < reader->spine_count) {
        int page_idx = find_current_page_index(reader);
        int *page_offsets = reader->page_cache[reader->current_chapter];

        if (page_offsets && page_idx > 0) {
            /* Move to previous page in same chapter */
            reader->current_offset = page_offsets[page_idx - 1];
            ui_ebook_reader_display_page(reader);

            /* Save progress */
            if (reader->is_epub && reader->reader) {
                update_epub_config_font_path(reader);
                epub_save_progress(reader->reader, reader->current_chapter, page_idx - 1, reader->current_offset);
                save_lvgl_config_to_cache(reader);
            }
            return true;
        } else {
            /* Beginning of chapter, try previous chapter */
            if (reader->is_epub) {
                return ui_ebook_reader_prev_chapter(reader);
            }
            return false;
        }
    } else {
        /* No cache, calculate on the fly */
        int new_offset = find_prev_page_start(reader, reader->current_offset);
        if (new_offset < 0 || new_offset >= reader->current_offset) {
            if (reader->is_epub) {
                return ui_ebook_reader_prev_chapter(reader);
            }
            return false;
        }
        reader->current_offset = new_offset;
        ui_ebook_reader_display_page(reader);

        /* Save progress */
        if (reader->is_epub && reader->reader) {
            update_epub_config_font_path(reader);
            int page_idx = find_current_page_index(reader);
            epub_save_progress(reader->reader, reader->current_chapter, page_idx, reader->current_offset);
            save_lvgl_config_to_cache(reader);
        }
        return true;
    }
}

/* Navigate to next chapter */
bool ui_ebook_reader_next_chapter(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->is_epub) {
        return false;
    }

    int next_chapter = reader->current_chapter + 1;
    bool loaded = false;

    while (next_chapter < reader->spine_count && !loaded) {
        if (load_chapter(reader, next_chapter)) {
            loaded = true;
        } else {
            next_chapter++;
        }
    }

    if (loaded) {
        ui_ebook_reader_display_page(reader);
        if (reader->reader) {
            update_epub_config_font_path(reader);
            epub_save_progress(reader->reader, reader->current_chapter, 0, 0);
            save_lvgl_config_to_cache(reader);
        }
        return true;
    }

    return false;
}

/* Navigate to previous chapter */
bool ui_ebook_reader_prev_chapter(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->is_epub) {
        return false;
    }

    int prev_chapter = reader->current_chapter - 1;
    bool loaded = false;

    while (prev_chapter >= 0 && !loaded) {
        if (load_chapter(reader, prev_chapter)) {
            /* Always start at the first page of the previous chapter */
            reader->current_offset = 0;
            loaded = true;
        } else {
            prev_chapter--;
        }
    }

    if (loaded) {
        ui_ebook_reader_display_page(reader);
        if (reader->reader) {
            update_epub_config_font_path(reader);
            epub_save_progress(reader->reader, reader->current_chapter, 0, 0);
            save_lvgl_config_to_cache(reader);
        }
        return true;
    }

    return false;
}

/* Increase font size - DISABLED: Font size is determined by font files */
/* Text size cannot be changed - it's determined by the font file itself */
void ui_ebook_reader_increase_font(ui_ebook_reader_t *reader)
{
    /* Font size cannot be changed - it's determined by the font file */
    /* To change text size, use a different font file */
    (void)reader; /* Suppress unused parameter warning */
}

/* Decrease font size - DISABLED: Font size is determined by font files */
/* Text size cannot be changed - it's determined by the font file itself */
void ui_ebook_reader_decrease_font(ui_ebook_reader_t *reader)
{
    /* Font size cannot be changed - it's determined by the font file */
    /* To change text size, use a different font file */
    (void)reader; /* Suppress unused parameter warning */
}

/* Jump to specific page */
void ui_ebook_reader_jump_to_page(ui_ebook_reader_t *reader, int page)
{
    if (!reader || !reader->current_text || page < 1) {
        return;
    }

    /* Use cache if available */
    if (reader->page_cache_valid && reader->page_cache && reader->page_count_per_chapter) {
        int target_page = page - 1; /* Convert to 0-based */
        int current_ch = 0;
        int page_offset = 0;

        /* Find which chapter contains the target page */
        int max_chapters = reader->is_epub ? reader->spine_count : 1;
        for (int ch = 0; ch < max_chapters; ch++) {
            int pages_in_ch = reader->page_count_per_chapter[ch];
            if (target_page < page_offset + pages_in_ch) {
                current_ch = ch;
                break;
            }
            page_offset += pages_in_ch;
        }

        /* Load the chapter if needed */
        if (current_ch != reader->current_chapter) {
            if (!load_chapter(reader, current_ch)) {
                return;
            }
        }

        /* Get the page offset from cache */
        int page_idx_in_ch = target_page - page_offset;
        int *page_offsets = reader->page_cache[current_ch];
        int page_count = reader->page_count_per_chapter[current_ch];

        if (page_offsets && page_idx_in_ch >= 0 && page_idx_in_ch < page_count) {
            reader->current_offset = page_offsets[page_idx_in_ch];
            ui_ebook_reader_display_page(reader);
            return;
        }
    }

    /* Fallback to calculation */
    int chars_per_page = reader->config.chars_per_page;
    int target_offset = (page - 1) * chars_per_page;
    if (target_offset < 0) {
        target_offset = 0;
    }
    if (target_offset > (int)reader->current_text_len) {
        target_offset = reader->current_text_len;
    }
    reader->current_offset = target_offset;
    ui_ebook_reader_display_page(reader);
}

/* Get current page number */
int ui_ebook_reader_get_current_page(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->current_text) {
        return 1;
    }

    /* Use cache if available */
    if (reader->page_cache_valid && reader->page_cache &&
        reader->current_chapter >= 0 && reader->current_chapter < reader->spine_count) {
        int page_idx = find_current_page_index(reader);
        int page_num = page_idx + 1;

        /* Add pages from previous chapters */
        for (int ch = 0; ch < reader->current_chapter; ch++) {
            if (reader->page_count_per_chapter && reader->page_count_per_chapter[ch] > 0) {
                page_num += reader->page_count_per_chapter[ch];
            }
        }
        return page_num;
    }

    /* Fallback to calculation */
    int chars_per_page = reader->config.chars_per_page;
    return (reader->current_offset / chars_per_page) + 1;
}

/* Get total pages */
int ui_ebook_reader_get_total_pages(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->current_text) {
        return 1;
    }

    /* Use cache if available */
    if (reader->page_cache_valid && reader->page_count_per_chapter) {
        int total = 0;
        int max_chapters = reader->is_epub ? reader->spine_count : 1;
        for (int ch = 0; ch < max_chapters; ch++) {
            if (reader->page_count_per_chapter[ch] > 0) {
                total += reader->page_count_per_chapter[ch];
            }
        }
        return total > 0 ? total : 1;
    }

    /* Fallback to calculation */
    int chars_per_page = reader->config.chars_per_page;
    int pages = (reader->current_text_len + chars_per_page - 1) / chars_per_page;
    return pages > 0 ? pages : 1;
}

/* Set line spacing */
/* NOTE: Changing line spacing triggers full EPUB re-indexing and page split recalculation */
void ui_ebook_reader_set_line_spacing(ui_ebook_reader_t *reader, ui_ebook_line_spacing_t spacing)
{
    if (!reader) {
        return;
    }

    if (reader->display.line_spacing != spacing) {
        reader->display.line_spacing = spacing;
        /* Update style will rebuild cache - entire EPUB is re-indexed */
        update_text_style(reader);
    }
}

/* Set letter spacing */
/* NOTE: Changing letter spacing triggers full EPUB re-indexing and page split recalculation */
void ui_ebook_reader_set_letter_spacing(ui_ebook_reader_t *reader, int spacing)
{
    if (!reader) {
        return;
    }

    if (reader->display.letter_spacing != spacing) {
        reader->display.letter_spacing = spacing;
        /* Update style will rebuild cache - entire EPUB is re-indexed */
        update_text_style(reader);
    }
}

/* Create settings page */
static void create_settings_page(ui_ebook_reader_t *reader)
{
    if (!reader || reader->settings_overlay) {
        return;
    }

    /* Get container */
    lv_obj_t *container = lv_obj_get_parent(reader->text_label);
    if (!container) {
        return;
    }

    /* Create overlay that covers the entire screen */
    reader->settings_overlay = lv_obj_create(container);
    lv_obj_set_size(reader->settings_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(reader->settings_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(reader->settings_overlay, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(reader->settings_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(reader->settings_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->settings_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(reader->settings_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(reader->settings_overlay);

    /* Create left panel for menu (50% width) */
    reader->settings_left_panel = lv_obj_create(reader->settings_overlay);
    lv_obj_set_size(reader->settings_left_panel, LV_PCT(50), LV_PCT(100));
    lv_obj_align(reader->settings_left_panel, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(reader->settings_left_panel, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(reader->settings_left_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(reader->settings_left_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(reader->settings_left_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->settings_left_panel, 10, LV_PART_MAIN);
    lv_obj_clear_flag(reader->settings_left_panel, LV_OBJ_FLAG_CLICKABLE);

    /* Create right panel for stats (50% width) */
    reader->settings_right_panel = lv_obj_create(reader->settings_overlay);
    lv_obj_set_size(reader->settings_right_panel, LV_PCT(50), LV_PCT(100));
    lv_obj_align(reader->settings_right_panel, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(reader->settings_right_panel, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(reader->settings_right_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(reader->settings_right_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->settings_right_panel, 20, LV_PART_MAIN);
    lv_obj_clear_flag(reader->settings_right_panel, LV_OBJ_FLAG_CLICKABLE);

    /* Create menu list in left panel */
    reader->settings_menu_list = lv_list_create(reader->settings_left_panel);
    // Disable default selection highlight for list widget
    lv_obj_set_style_bg_opa(reader->settings_menu_list, LV_OPA_TRANSP, LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_bg_color(reader->settings_menu_list, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_size(reader->settings_menu_list, LV_PCT(100), LV_PCT(100));
    lv_obj_align(reader->settings_menu_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(reader->settings_menu_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(reader->settings_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->settings_menu_list, 5, LV_PART_MAIN);
    lv_obj_add_flag(reader->settings_menu_list, LV_OBJ_FLAG_CLICKABLE);
    /* Disable scrolling - no extended text scrolling */
    lv_obj_clear_flag(reader->settings_menu_list, LV_OBJ_FLAG_SCROLLABLE);

    /* Add menu items */
    lv_obj_t *btn;

    if (reader->is_epub) {
        btn = lv_list_add_btn(reader->settings_menu_list, NULL, "Previous Chapter");
        lv_obj_set_user_data(btn, (void *)1);
        lv_obj_add_event_cb(btn, settings_menu_item_event_cb, LV_EVENT_CLICKED, reader);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);
        lv_obj_set_style_margin_bottom(btn, 5, LV_PART_MAIN);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        btn = lv_list_add_btn(reader->settings_menu_list, NULL, "Next Chapter");
        lv_obj_set_user_data(btn, (void *)2);
        lv_obj_add_event_cb(btn, settings_menu_item_event_cb, LV_EVENT_CLICKED, reader);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);
        lv_obj_set_style_margin_bottom(btn, 5, LV_PART_MAIN);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    }

    btn = lv_list_add_btn(reader->settings_menu_list, NULL, "Font Settings");
    lv_obj_set_user_data(btn, (void *)3);
    lv_obj_add_event_cb(btn, settings_menu_item_event_cb, LV_EVENT_CLICKED, reader);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);
    /* Explicitly set focused state to use inverse colors (no blue) */
    lv_obj_set_style_bg_color(btn, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(btn, 5, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    /* Create Line Spacing button with current value */
    char line_spacing_text[64];
    const char *spacing_label;
    switch (reader->display.line_spacing) {
        case UI_EBOOK_LINE_SPACING_TIGHT:
            spacing_label = "1.0";
            break;
        case UI_EBOOK_LINE_SPACING_MID:
            spacing_label = "1.2";
            break;
        case UI_EBOOK_LINE_SPACING_LOOSE:
            spacing_label = "1.5";
            break;
        default:
            spacing_label = "1.0";
            break;
    }
    snprintf(line_spacing_text, sizeof(line_spacing_text), "Line Spacing: %s", spacing_label);
    btn = lv_list_add_btn(reader->settings_menu_list, NULL, line_spacing_text);
    lv_obj_set_user_data(btn, (void *)4);
    lv_obj_add_event_cb(btn, settings_menu_item_event_cb, LV_EVENT_CLICKED, reader);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(btn, 5, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    /* Create stats label in right panel */
    reader->settings_stats_label = lv_label_create(reader->settings_right_panel);
    lv_obj_set_size(reader->settings_stats_label, LV_PCT(100), LV_PCT(100));
    lv_obj_align(reader->settings_stats_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(reader->settings_stats_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(reader->settings_stats_label, &ui_font_terminus_bold_24_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(reader->settings_stats_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->settings_stats_label, 10, LV_PART_MAIN);

    /* Update stats */
    update_settings_stats(reader);

    /* Initially hide the settings page */
    lv_obj_add_flag(reader->settings_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* Update settings stats display */
static void update_settings_stats(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->settings_stats_label) {
        return;
    }

    int current_page = ui_ebook_reader_get_current_page(reader);
    int total_pages = ui_ebook_reader_get_total_pages(reader);

    /* Calculate total progress percentage */
    float total_progress = 0.0f;
    if (total_pages > 0) {
        total_progress = (float)current_page * 100.0f / (float)total_pages;
    }

    /* Calculate chapter progress percentage */
    float chapter_progress = 0.0f;
    if (reader->is_epub && reader->page_cache_valid &&
        reader->current_chapter >= 0 && reader->current_chapter < reader->spine_count) {
        int *page_offsets = reader->page_cache[reader->current_chapter];
        int page_count = reader->page_count_per_chapter[reader->current_chapter];
        if (page_offsets && page_count > 0) {
            int chapter_page = 0;
            for (int i = 0; i < page_count; i++) {
                if (page_offsets[i] <= reader->current_offset) {
                    chapter_page = i;
                } else {
                    break;
                }
            }
            chapter_progress = (float)chapter_page * 100.0f / (float)page_count;
        }
    } else {
        /* For TXT files, use overall progress */
        chapter_progress = total_progress;
    }

    char stats_text[512];

    if (reader->is_epub) {
        /* Format with chapter information - evenly distributed sections */
        snprintf(stats_text, sizeof(stats_text),
                 "Statistics\n"
                 "\n"
                 "Total\n"
                 "%.1f%%\n"
                 "%d/%d\n"
                 "\n"
                 "\n"
                 "Chapter\n"
                 "%.1f%%\n"
                 "%d/%d",
                 total_progress,
                 current_page, total_pages,
                 chapter_progress,
                 reader->current_chapter + 1, reader->spine_count);
    } else {
        /* Format for TXT files (no chapter info) */
        snprintf(stats_text, sizeof(stats_text),
                 "Statistics\n"
                 "Total\n"
                 "%.1f%%\n"
                 "%d/%d",
                 total_progress,
                 current_page, total_pages);
    }

    lv_label_set_text(reader->settings_stats_label, stats_text);
}

/* Settings menu item event callback */
static void settings_menu_item_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *btn = lv_event_get_current_target(e);
    ui_ebook_reader_t *reader = (ui_ebook_reader_t *)lv_event_get_user_data(e);
    int action = (int)(intptr_t)lv_obj_get_user_data(btn);

    switch (action) {
    case 1: /* Previous Chapter */
        if (reader->is_epub) {
            ui_ebook_reader_prev_chapter(reader);
            update_settings_stats(reader);
        }
        break;
    case 2: /* Next Chapter */
        if (reader->is_epub) {
            ui_ebook_reader_next_chapter(reader);
            update_settings_stats(reader);
        }
        break;
    case 3: /* Font Settings */
        /* Show font selection page */
        ui_ebook_reader_show_font_selection(reader);
        break;
    case 4: /* Line Spacing (Disabled) */
        /* Disabled - do nothing */
        break;
    }
}

/* Show settings page */
void ui_ebook_reader_show_settings(ui_ebook_reader_t *reader)
{
    if (!reader) {
        return;
    }

    /* Create settings page if it doesn't exist */
    if (!reader->settings_overlay) {
        create_settings_page(reader);
    }

    if (reader->settings_overlay) {
        /* Show settings page */
        lv_obj_clear_flag(reader->settings_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(reader->settings_overlay);

        /* Update stats */
        update_settings_stats(reader);

        /* Update line spacing button text to show current value */
        if (reader->settings_menu_list) {
            uint32_t child_cnt = lv_obj_get_child_cnt(reader->settings_menu_list);
            for (uint32_t i = 0; i < child_cnt; i++) {
                lv_obj_t *item = lv_obj_get_child(reader->settings_menu_list, i);
                if (item) {
                    int action = (int)(intptr_t)lv_obj_get_user_data(item);
                    if (action == 4) { /* Line Spacing button */
                        const char *spacing_label;
                        switch (reader->display.line_spacing) {
                            case UI_EBOOK_LINE_SPACING_TIGHT:
                                spacing_label = "1.0";
                                break;
                            case UI_EBOOK_LINE_SPACING_MID:
                                spacing_label = "1.2";
                                break;
                            case UI_EBOOK_LINE_SPACING_LOOSE:
                                spacing_label = "1.5";
                                break;
                            default:
                                spacing_label = "1.0";
                                break;
                        }
                        char line_spacing_text[64];
                        snprintf(line_spacing_text, sizeof(line_spacing_text), "Line Spacing: %s", spacing_label);
                        lv_obj_t *label = lv_obj_get_child(item, 0);
                        if (label && lv_obj_check_type(label, &lv_label_class)) {
                            lv_label_set_text(label, line_spacing_text);
                        }
                        break;
                    }
                }
            }
        }

        /* Focus on menu list */
        if (reader->settings_menu_list) {
            /* Set first item as selected */
            reader->settings_selected_idx = 0;
            uint32_t child_cnt = lv_obj_get_child_cnt(reader->settings_menu_list);
            if (child_cnt > 0) {
                /* Clear all previous highlights */
                for (uint32_t i = 0; i < child_cnt; i++) {
                    lv_obj_t *item = lv_obj_get_child(reader->settings_menu_list, i);
                    if (item) {
                        lv_obj_clear_state(item, LV_STATE_FOCUSED);
                        lv_obj_set_style_bg_color(item, lv_color_white(), LV_PART_MAIN);
                        lv_obj_set_style_text_color(item, lv_color_black(), LV_PART_MAIN);
                        lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
                        lv_obj_set_style_border_color(item, lv_color_black(), LV_PART_MAIN);
                        lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                    }
                }
                /* Highlight first item with inverse colors */
                lv_obj_t *first_item = lv_obj_get_child(reader->settings_menu_list, 0);
                if (first_item) {
                    /* Clear focused state first to remove any default blue colors */
                    lv_obj_clear_state(first_item, LV_STATE_FOCUSED);
                    /* Set inverse colors (black bg, white text) */
                    lv_obj_set_style_bg_color(first_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_bg_opa(first_item, LV_OPA_COVER, LV_PART_MAIN);
                    lv_obj_set_style_text_color(first_item, lv_color_white(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(first_item, 2, LV_PART_MAIN);
                    lv_obj_set_style_border_color(first_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_opa(first_item, LV_OPA_COVER, LV_PART_MAIN);
                    /* Explicitly clear any default focused state colors */
                    lv_obj_set_style_bg_color(first_item, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
                    lv_obj_set_style_text_color(first_item, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
                    /* Now add focused state */
                    lv_obj_add_state(first_item, LV_STATE_FOCUSED);
                }
            }
        }
    }
}

/* Hide settings page */
void ui_ebook_reader_hide_settings(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->settings_overlay) {
        return;
    }

    /* Hide settings page */
    lv_obj_add_flag(reader->settings_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* Handle keyboard input in settings page */
void ui_ebook_reader_settings_handle_key(ui_ebook_reader_t *reader, lv_key_t key)
{
    if (!reader || !reader->settings_overlay) {
        return;
    }

    /* Check if settings page is visible */
    if (lv_obj_has_flag(reader->settings_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    LV_LOG_USER("Settings handle_key: key=%d", key);

    if (!reader->settings_menu_list) {
        return;
    }

    uint32_t child_cnt = lv_obj_get_child_cnt(reader->settings_menu_list);
    if (child_cnt == 0) {
        return;
    }

    switch (key) {
    case LV_KEY_ENTER:
        /* Execute action on selected menu item */
        if (reader->settings_selected_idx >= 0 && reader->settings_selected_idx < (int)child_cnt) {
            /* Get the button from the list - lv_list stores buttons as children */
            lv_obj_t *selected_item = lv_obj_get_child(reader->settings_menu_list, reader->settings_selected_idx);
            if (selected_item) {
                /* Get action from user data and execute it */
                int action = (int)(intptr_t)lv_obj_get_user_data(selected_item);
                LV_LOG_USER("Settings menu ENTER: selected_idx=%d, action=%d", reader->settings_selected_idx, action);
                switch (action) {
                case 1: /* Previous Chapter */
                    if (reader->is_epub) {
                        ui_ebook_reader_prev_chapter(reader);
                        update_settings_stats(reader);
                    }
                    break;
                case 2: /* Next Chapter */
                    if (reader->is_epub) {
                        ui_ebook_reader_next_chapter(reader);
                        update_settings_stats(reader);
                    }
                    break;
                case 3: /* Font Settings */
                    /* Show font selection page */
                    ui_ebook_reader_show_font_selection(reader);
                    break;
                case 4: /* Line Spacing */
                    {
                        /* Cycle through line spacing values: 1.0 -> 1.2 -> 1.5 -> 1.0 */
                        ui_ebook_line_spacing_t new_spacing;
                        const char *spacing_label;
                        switch (reader->display.line_spacing) {
                            case UI_EBOOK_LINE_SPACING_TIGHT:
                                new_spacing = UI_EBOOK_LINE_SPACING_MID;
                                spacing_label = "1.2";
                                break;
                            case UI_EBOOK_LINE_SPACING_MID:
                                new_spacing = UI_EBOOK_LINE_SPACING_LOOSE;
                                spacing_label = "1.5";
                                break;
                            case UI_EBOOK_LINE_SPACING_LOOSE:
                                new_spacing = UI_EBOOK_LINE_SPACING_TIGHT;
                                spacing_label = "1.0";
                                break;
                            default:
                                new_spacing = UI_EBOOK_LINE_SPACING_TIGHT;
                                spacing_label = "1.0";
                                break;
                        }

                        /* Update line spacing */
                        reader->display.line_spacing = new_spacing;

                        /* Update display calculations */
                        update_display_calculations(&reader->display);

                        /* Update text style (this will rebuild page cache and redisplay) */
                        update_text_style(reader);

                        /* Update button text */
                        char line_spacing_text[64];
                        snprintf(line_spacing_text, sizeof(line_spacing_text), "Line Spacing: %s", spacing_label);
                        lv_obj_t *line_spacing_btn = lv_obj_get_child(reader->settings_menu_list, reader->settings_selected_idx);
                        if (line_spacing_btn) {
                            lv_obj_t *label = lv_obj_get_child(line_spacing_btn, 0);
                            if (label && lv_obj_check_type(label, &lv_label_class)) {
                                lv_label_set_text(label, line_spacing_text);
                            }
                        }

                        /* Save to cache */
                        if (reader->is_epub && reader->reader) {
                            update_epub_config_font_path(reader);
                            int page_idx = find_current_page_index(reader);
                            epub_save_progress(reader->reader, reader->current_chapter, page_idx, reader->current_offset);
                            save_lvgl_config_to_cache(reader);
                        }

                        /* Update stats */
                        update_settings_stats(reader);
                    }
                    break;
                default:
                    LV_LOG_WARN("Unknown menu action: %d", action);
                    break;
                }
            } else {
                LV_LOG_WARN("Settings menu ENTER: selected_item is NULL for idx=%d", reader->settings_selected_idx);
            }
        } else {
            LV_LOG_WARN("Settings menu ENTER: invalid selected_idx=%d, child_cnt=%u", reader->settings_selected_idx, child_cnt);
        }
        break;

    case LV_KEY_ESC:
        /* Return to reader UI */
        ui_ebook_reader_hide_settings(reader);
        break;

    case LV_KEY_UP:
    case LV_KEY_LEFT:
        /* Navigate list up */
        if (reader->settings_selected_idx > 0) {
            /* Clear previous selection highlight */
            if (reader->settings_selected_idx < (int)child_cnt) {
                lv_obj_t *prev_item = lv_obj_get_child(reader->settings_menu_list, reader->settings_selected_idx);
                if (prev_item) {
                    lv_obj_clear_state(prev_item, LV_STATE_FOCUSED);
                    lv_obj_set_style_bg_color(prev_item, lv_color_white(), LV_PART_MAIN);
                    lv_obj_set_style_text_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(prev_item, 2, LV_PART_MAIN);
                    lv_obj_set_style_border_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_opa(prev_item, LV_OPA_COVER, LV_PART_MAIN);
                }
            }
            reader->settings_selected_idx--;
            lv_obj_t *item = lv_obj_get_child(reader->settings_menu_list, reader->settings_selected_idx);
            if (item) {
                /* Clear focused state first */
                lv_obj_clear_state(item, LV_STATE_FOCUSED);
                /* Set inverse colors */
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                /* Explicitly set focused state colors */
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
                /* Now add focused state */
                lv_obj_add_state(item, LV_STATE_FOCUSED);
            }
        }
        break;

    case LV_KEY_DOWN:
    case LV_KEY_RIGHT:
        /* Navigate list down */
        if (reader->settings_selected_idx < (int)child_cnt - 1) {
            /* Clear previous selection highlight */
            if (reader->settings_selected_idx >= 0 && reader->settings_selected_idx < (int)child_cnt) {
                lv_obj_t *prev_item = lv_obj_get_child(reader->settings_menu_list, reader->settings_selected_idx);
                if (prev_item) {
                    lv_obj_clear_state(prev_item, LV_STATE_FOCUSED);
                    lv_obj_set_style_bg_color(prev_item, lv_color_white(), LV_PART_MAIN);
                    lv_obj_set_style_text_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(prev_item, 2, LV_PART_MAIN);
                    lv_obj_set_style_border_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_opa(prev_item, LV_OPA_COVER, LV_PART_MAIN);
                }
            }
            reader->settings_selected_idx++;
            lv_obj_t *item = lv_obj_get_child(reader->settings_menu_list, reader->settings_selected_idx);
            if (item) {
                /* Clear focused state first */
                lv_obj_clear_state(item, LV_STATE_FOCUSED);
                /* Set inverse colors */
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                /* Explicitly set focused state colors */
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
                /* Now add focused state */
                lv_obj_add_state(item, LV_STATE_FOCUSED);
            }
        }
        break;

    default:
        break;
    }
}

/* Handle keyboard input */
void ui_ebook_reader_handle_key(ui_ebook_reader_t *reader, lv_key_t key)
{
    if (!reader) {
        return;
    }

    /* Check if font selection page is visible - route keys to font selection handler */
    if (reader->font_selection_overlay && !lv_obj_has_flag(reader->font_selection_overlay, LV_OBJ_FLAG_HIDDEN)) {
        font_selection_handle_key(reader, key);
        return;
    }

    /* Check if settings page is visible - route keys to settings handler */
    if (reader->settings_overlay && !lv_obj_has_flag(reader->settings_overlay, LV_OBJ_FLAG_HIDDEN)) {
        ui_ebook_reader_settings_handle_key(reader, key);
        return;
    }

    /* Settings page not visible - handle reader navigation */
    if (key == LV_KEY_ENTER) {
        /* Open settings page */
        ui_ebook_reader_show_settings(reader);
        return;
    }

    switch (key) {
    case LV_KEY_LEFT:
        ui_ebook_reader_prev_page(reader);
        break;
    case LV_KEY_RIGHT:
        ui_ebook_reader_next_page(reader);
        break;
    case LV_KEY_ESC:
        /* Save reading progress and return to folder screen */
        if (reader->is_epub && reader->reader) {
            /* Save progress for EPUB */
            int page_idx = 0;
            if (reader->page_cache_valid && reader->page_cache &&
                reader->current_chapter >= 0 && reader->current_chapter < reader->spine_count) {
                page_idx = find_current_page_index(reader);
            }
            update_epub_config_font_path(reader);
            epub_save_progress(reader->reader, reader->current_chapter, page_idx, reader->current_offset);
            save_lvgl_config_to_cache(reader);
        }
        /* Return to folder screen */
        _ui_screen_change(&ui_floder_screen, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_floder_screen_screen_init);
        break;
    }
}

/* Scan font directory for .bin files */
static void scan_font_directory(ui_ebook_reader_t *reader)
{
    if (!reader) {
        return;
    }

    /* Free existing font list */
    if (reader->font_list) {
        for (int i = 0; i < reader->font_list_count; i++) {
            if (reader->font_list[i]) {
                lv_free(reader->font_list[i]);
            }
        }
        lv_free(reader->font_list);
        reader->font_list = NULL;
        reader->font_list_count = 0;
    }

    /* Open directory */
    lv_fs_dir_t dir;
    lv_fs_res_t res = lv_fs_dir_open(&dir, "A:/fonts");
    if (res != LV_FS_RES_OK) {
        LV_LOG_ERROR("Failed to open font directory A:/fonts");
        return;
    }

    /* Count .bin files first */
    int count = 0;
    char fn[256];
    while (1) {
        res = lv_fs_dir_read(&dir, fn, sizeof(fn));
        if (res != LV_FS_RES_OK || fn[0] == '\0') {
            break;
        }

        /* Check if file ends with .bin */
        int len = strlen(fn);
        if (len > 4 && strcmp(fn + len - 4, ".bin") == 0) {
            count++;
        }
    }

    lv_fs_dir_close(&dir);

    if (count == 0) {
        LV_LOG_WARN("No .bin font files found in A:/fonts");
        return;
    }

    /* Allocate font list */
    reader->font_list = lv_malloc(sizeof(char *) * count);
    if (!reader->font_list) {
        LV_LOG_ERROR("Failed to allocate font list");
        return;
    }

    /* Read font filenames */
    res = lv_fs_dir_open(&dir, "A:/fonts");
    if (res != LV_FS_RES_OK) {
        lv_free(reader->font_list);
        reader->font_list = NULL;
        return;
    }

    int idx = 0;
    char fn2[256];
    while (1) {
        res = lv_fs_dir_read(&dir, fn2, sizeof(fn2));
        if (res != LV_FS_RES_OK || fn2[0] == '\0') {
            break;
        }

        /* Check if file ends with .bin */
        int len = strlen(fn2);
        if (len > 4 && strcmp(fn2 + len - 4, ".bin") == 0) {
            /* Allocate and store filename */
            reader->font_list[idx] = lv_malloc(strlen(fn2) + 1);
            if (reader->font_list[idx]) {
                strcpy(reader->font_list[idx], fn2);
                idx++;
            }
        }
    }

    lv_fs_dir_close(&dir);
    reader->font_list_count = idx;

    /* Sort font list alphabetically */
    for (int i = 0; i < reader->font_list_count - 1; i++) {
        for (int j = i + 1; j < reader->font_list_count; j++) {
            if (strcmp(reader->font_list[i], reader->font_list[j]) > 0) {
                char *tmp = reader->font_list[i];
                reader->font_list[i] = reader->font_list[j];
                reader->font_list[j] = tmp;
            }
        }
    }
}

/* Create font selection page */
static void create_font_selection_page(ui_ebook_reader_t *reader)
{
    if (!reader || reader->font_selection_overlay) {
        return;
    }

    /* Get container */
    lv_obj_t *container = lv_obj_get_parent(reader->text_label);
    if (!container) {
        return;
    }

    /* Scan font directory */
    scan_font_directory(reader);

    if (reader->font_list_count == 0) {
        LV_LOG_WARN("No fonts found to display");
        return;
    }

    /* Create overlay */
    reader->font_selection_overlay = lv_obj_create(container);
    lv_obj_set_size(reader->font_selection_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(reader->font_selection_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(reader->font_selection_overlay, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(reader->font_selection_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(reader->font_selection_overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(reader->font_selection_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->font_selection_overlay, 10, LV_PART_MAIN);
    lv_obj_clear_flag(reader->font_selection_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(reader->font_selection_overlay);

    /* Create title label */
    lv_obj_t *title_label = lv_label_create(reader->font_selection_overlay);
    lv_label_set_text(title_label, "Select Font");
    lv_obj_set_style_text_font(title_label, &ui_font_terminus_bold_16_2, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);

    /* Create font list */
    reader->font_selection_list = lv_list_create(reader->font_selection_overlay);
    lv_obj_set_size(reader->font_selection_list, LV_PCT(100), LV_PCT(85));
    lv_obj_align(reader->font_selection_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(reader->font_selection_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(reader->font_selection_list, LV_OPA_TRANSP, LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_bg_color(reader->font_selection_list, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
    lv_obj_set_style_border_width(reader->font_selection_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(reader->font_selection_list, 5, LV_PART_MAIN);
    lv_obj_add_flag(reader->font_selection_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(reader->font_selection_list, LV_OBJ_FLAG_SCROLLABLE);

    /* Add font items to list */
    for (int i = 0; i < reader->font_list_count; i++) {
        /* Remove .bin suffix for display */
        char display_name[256];
        strncpy(display_name, reader->font_list[i], sizeof(display_name) - 1);
        display_name[sizeof(display_name) - 1] = '\0';
        int len = strlen(display_name);
        if (len > 4 && strcmp(display_name + len - 4, ".bin") == 0) {
            display_name[len - 4] = '\0';
        }

        lv_obj_t *btn = lv_list_add_btn(reader->font_selection_list, NULL, display_name);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, font_selection_item_event_cb, LV_EVENT_CLICKED, reader);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);
        /* Explicitly set focused state to use inverse colors (no blue) */
        lv_obj_set_style_bg_color(btn, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);
        lv_obj_set_style_margin_bottom(btn, 5, LV_PART_MAIN);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* Initially hide the font selection page */
    lv_obj_add_flag(reader->font_selection_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* Font selection item event callback */
static void font_selection_item_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *btn = lv_event_get_current_target(e);
    ui_ebook_reader_t *reader = (ui_ebook_reader_t *)lv_event_get_user_data(e);
    int font_idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (!reader || font_idx < 0 || font_idx >= reader->font_list_count) {
        return;
    }

    /* Build full font path */
    char font_path[512];
    snprintf(font_path, sizeof(font_path), "A:/fonts/%s", reader->font_list[font_idx]);

    /* Update font path */
    strncpy(reader->display.font_path, font_path, sizeof(reader->display.font_path) - 1);
    reader->display.font_path[sizeof(reader->display.font_path) - 1] = '\0';

    /* Destroy old font */
    if (reader->display.font && reader->display.font != (lv_font_t *)&ui_font_JetBrains_Mono_Regular_16_2) {
        lv_binfont_destroy(reader->display.font);
    }

    /* Load new font */
    reader->display.font = lv_binfont_create(font_path);
    if (!reader->display.font) {
        LV_LOG_ERROR("Failed to load font from %s, using fallback", font_path);
        reader->display.font = (lv_font_t *)&ui_font_JetBrains_Mono_Regular_16_2;
    } else {
        LV_LOG_USER("Successfully loaded font from %s", font_path);
    }

    /* Update display calculations and rebuild cache */
    update_display_calculations(&reader->display);
    calculate_text_area(reader);
    update_text_style(reader);

    /* Rebuild page cache with new font */
    if (reader->is_epub) {
        free_page_cache(reader);
        build_page_cache(reader);
    }

    /* Redisplay current page */
    ui_ebook_reader_display_page(reader);

    /* Hide font selection and return to settings */
    ui_ebook_reader_hide_font_selection(reader);
    ui_ebook_reader_show_settings(reader);
}

/* Handle keyboard input in font selection page */
static void font_selection_handle_key(ui_ebook_reader_t *reader, lv_key_t key)
{
    if (!reader || !reader->font_selection_overlay) {
        return;
    }

    /* Check if font selection page is visible */
    if (lv_obj_has_flag(reader->font_selection_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    if (!reader->font_selection_list) {
        return;
    }

    uint32_t child_cnt = lv_obj_get_child_cnt(reader->font_selection_list);
    if (child_cnt == 0) {
        return;
    }

    switch (key) {
    case LV_KEY_ENTER: {
        /* Execute action on selected font item */
        if (reader->font_selection_selected_idx >= 0 && reader->font_selection_selected_idx < (int)child_cnt) {
            lv_obj_t *selected_item = lv_obj_get_child(reader->font_selection_list, reader->font_selection_selected_idx);
            if (selected_item) {
                /* Get font index and call the callback directly */
                int font_idx = (int)(intptr_t)lv_obj_get_user_data(selected_item);
                LV_LOG_USER("Font selection ENTER: selected_idx=%d, font_idx=%d", reader->font_selection_selected_idx, font_idx);
                if (font_idx >= 0 && font_idx < reader->font_list_count) {
                    /* Build full font path */
                    char font_path[512];
                    snprintf(font_path, sizeof(font_path), "A:/fonts/%s", reader->font_list[font_idx]);

                    /* Update font path */
                    strncpy(reader->display.font_path, font_path, sizeof(reader->display.font_path) - 1);
                    reader->display.font_path[sizeof(reader->display.font_path) - 1] = '\0';

                    /* Destroy old font */
                    if (reader->display.font && reader->display.font != (lv_font_t *)&ui_font_JetBrains_Mono_Regular_16_2) {
                        lv_binfont_destroy(reader->display.font);
                    }

                    /* Load new font */
                    reader->display.font = lv_binfont_create(font_path);
                    if (!reader->display.font) {
                        LV_LOG_ERROR("Failed to load font from %s, using fallback", font_path);
                        reader->display.font = (lv_font_t *)&ui_font_JetBrains_Mono_Regular_16_2;
                    } else {
                        LV_LOG_USER("Successfully loaded font from %s", font_path);
                    }

                    /* Update display calculations and rebuild cache */
                    update_display_calculations(&reader->display);
                    calculate_text_area(reader);
                    update_text_style(reader);

                    /* Rebuild page cache with new font */
                    if (reader->is_epub) {
                        free_page_cache(reader);
                        build_page_cache(reader);
                    }

                    /* Redisplay current page */
                    ui_ebook_reader_display_page(reader);

                    /* Hide font selection and return to settings */
                    ui_ebook_reader_hide_font_selection(reader);
                    ui_ebook_reader_show_settings(reader);
                }
            }
        }
        break;
    }

    case LV_KEY_ESC:
        /* Return to settings page */
        ui_ebook_reader_hide_font_selection(reader);
        ui_ebook_reader_show_settings(reader);
        break;

    case LV_KEY_UP:
    case LV_KEY_LEFT:
        /* Navigate list up */
        if (reader->font_selection_selected_idx > 0) {
            /* Clear previous selection highlight */
            if (reader->font_selection_selected_idx < (int)child_cnt) {
                lv_obj_t *prev_item = lv_obj_get_child(reader->font_selection_list, reader->font_selection_selected_idx);
                if (prev_item) {
                    lv_obj_clear_state(prev_item, LV_STATE_FOCUSED);
                    lv_obj_set_style_bg_color(prev_item, lv_color_white(), LV_PART_MAIN);
                    lv_obj_set_style_text_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(prev_item, 2, LV_PART_MAIN);
                    lv_obj_set_style_border_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_opa(prev_item, LV_OPA_COVER, LV_PART_MAIN);
                }
            }
            reader->font_selection_selected_idx--;
            lv_obj_t *item = lv_obj_get_child(reader->font_selection_list, reader->font_selection_selected_idx);
            if (item) {
                lv_obj_clear_state(item, LV_STATE_FOCUSED);
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
                lv_obj_add_state(item, LV_STATE_FOCUSED);
            }
        }
        break;

    case LV_KEY_DOWN:
    case LV_KEY_RIGHT:
        /* Navigate list down */
        if (reader->font_selection_selected_idx < (int)child_cnt - 1) {
            /* Clear previous selection highlight */
            if (reader->font_selection_selected_idx >= 0 && reader->font_selection_selected_idx < (int)child_cnt) {
                lv_obj_t *prev_item = lv_obj_get_child(reader->font_selection_list, reader->font_selection_selected_idx);
                if (prev_item) {
                    lv_obj_clear_state(prev_item, LV_STATE_FOCUSED);
                    lv_obj_set_style_bg_color(prev_item, lv_color_white(), LV_PART_MAIN);
                    lv_obj_set_style_text_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(prev_item, 2, LV_PART_MAIN);
                    lv_obj_set_style_border_color(prev_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_opa(prev_item, LV_OPA_COVER, LV_PART_MAIN);
                }
            }
            reader->font_selection_selected_idx++;
            lv_obj_t *item = lv_obj_get_child(reader->font_selection_list, reader->font_selection_selected_idx);
            if (item) {
                lv_obj_clear_state(item, LV_STATE_FOCUSED);
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(item, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_bg_color(item, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
                lv_obj_set_style_text_color(item, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
                lv_obj_add_state(item, LV_STATE_FOCUSED);
            }
        }
        break;

    default:
        break;
    }
}

/* Show font selection page */
void ui_ebook_reader_show_font_selection(ui_ebook_reader_t *reader)
{
    if (!reader) {
        return;
    }

    /* Hide settings page first */
    ui_ebook_reader_hide_settings(reader);

    /* Create font selection page if it doesn't exist */
    if (!reader->font_selection_overlay) {
        create_font_selection_page(reader);
    }

    if (reader->font_selection_overlay) {
        /* Show font selection page */
        lv_obj_clear_flag(reader->font_selection_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(reader->font_selection_overlay);

        /* Focus on font list */
        if (reader->font_selection_list) {
            /* Set first item as selected */
            reader->font_selection_selected_idx = 0;
            uint32_t child_cnt = lv_obj_get_child_cnt(reader->font_selection_list);
            if (child_cnt > 0) {
                /* Clear all previous highlights */
                for (uint32_t i = 0; i < child_cnt; i++) {
                    lv_obj_t *item = lv_obj_get_child(reader->font_selection_list, i);
                    if (item) {
                        lv_obj_clear_state(item, LV_STATE_FOCUSED);
                        lv_obj_set_style_bg_color(item, lv_color_white(), LV_PART_MAIN);
                        lv_obj_set_style_text_color(item, lv_color_black(), LV_PART_MAIN);
                        lv_obj_set_style_border_width(item, 2, LV_PART_MAIN);
                        lv_obj_set_style_border_color(item, lv_color_black(), LV_PART_MAIN);
                        lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_PART_MAIN);
                    }
                }
                /* Highlight first item with inverted colors */
                lv_obj_t *first_item = lv_obj_get_child(reader->font_selection_list, 0);
                if (first_item) {
                    lv_obj_clear_state(first_item, LV_STATE_FOCUSED);
                    lv_obj_set_style_bg_color(first_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_bg_opa(first_item, LV_OPA_COVER, LV_PART_MAIN);
                    lv_obj_set_style_text_color(first_item, lv_color_white(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(first_item, 2, LV_PART_MAIN);
                    lv_obj_set_style_border_color(first_item, lv_color_black(), LV_PART_MAIN);
                    lv_obj_set_style_border_opa(first_item, LV_OPA_COVER, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(first_item, lv_color_black(), LV_STATE_FOCUSED | LV_PART_MAIN);
                    lv_obj_set_style_text_color(first_item, lv_color_white(), LV_STATE_FOCUSED | LV_PART_MAIN);
                    lv_obj_add_state(first_item, LV_STATE_FOCUSED);
                }
            }
        }
    }
}

/* Hide font selection page */
void ui_ebook_reader_hide_font_selection(ui_ebook_reader_t *reader)
{
    if (!reader || !reader->font_selection_overlay) {
        return;
    }

    /* Hide font selection page */
    lv_obj_add_flag(reader->font_selection_overlay, LV_OBJ_FLAG_HIDDEN);
}
