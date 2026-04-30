/**
 * @file ui_ebook_reader.h
 * @brief LVGL-based EPUB/TXT ebook reader
 */

#ifndef UI_EBOOK_READER_H
#define UI_EBOOK_READER_H

#include "lvgl/lvgl.h"
#include "../epub_reader/include/epub_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Line spacing modes */
typedef enum {
    UI_EBOOK_LINE_SPACING_TIGHT = 0,
    UI_EBOOK_LINE_SPACING_MID,
    UI_EBOOK_LINE_SPACING_LOOSE
} ui_ebook_line_spacing_t;

/* Display settings context */
typedef struct {
    /* Font settings */
    char                    font_path[256];      /* Font file path */
    lv_font_t              *font;                /* Current font (dynamically loaded) - font size determined by this font */

    /* Spacing settings */
    ui_ebook_line_spacing_t line_spacing;        /* Line spacing mode */
    int32_t                 letter_spacing;      /* Letter spacing in pixels */
    float                   line_height_mult;    /* Line height multiplier (calculated) */

    /* Layout settings */
    int32_t                 margin_left;         /* Left margin in pixels */
    int32_t                 margin_right;        /* Right margin in pixels */
    int32_t                 margin_top;          /* Top margin in pixels */
    int32_t                 margin_bottom;       /* Bottom margin in pixels */

    /* Active text area (calculated) */
    int32_t                 text_area_width;     /* Available text width */
    int32_t                 text_area_height;    /* Available text height */

    /* Calculated values (derived from settings) */
    int32_t                 line_height;         /* Actual line height (font->line_height * line_height_mult) */
    int32_t                 line_spacing_px;      /* Line spacing in pixels (line_height - font->line_height) */
} ui_ebook_display_settings_t;

/* Ebook reader state */
typedef struct {
    epub_reader_t        *reader;          /* EPUB reader handle (NULL for TXT) */
    epub_config_t         config;          /* Reader configuration */
    int                   current_chapter; /* Current chapter index */
    int                   current_offset;  /* Current character offset */
    char                 *current_text;    /* Current chapter text */
    size_t                current_text_len;/* Current text length */
    int                   spine_count;     /* Number of spine items */
    epub_content_item_t **spine_items;    /* Spine items array */
    epub_toc_entry_t     *toc;            /* Table of contents */

    /* For TXT files */
    char                 *txt_filepath;    /* TXT file path (if not EPUB) */

    /* Display settings context */
    ui_ebook_display_settings_t display;   /* Display settings and layout */
    int                   total_pages;     /* Cached total pages */

    /* Page boundary cache: chapter_index -> array of page start offsets */
    int                  **page_cache;     /* 2D array: [chapter][page] = offset */
    int                   *page_count_per_chapter; /* Number of pages per chapter */
    bool                  page_cache_valid; /* Whether page cache is valid */

    /* LVGL objects */
    lv_obj_t             *text_label;     /* Text display label */
    lv_obj_t             *progress_line;   /* Progress line (2px line) */
    lv_obj_t             *char_boxes_container; /* Container for character boxes overlay */

    /* Settings page */
    lv_obj_t             *settings_overlay; /* Settings page overlay */
    lv_obj_t             *settings_left_panel; /* Left panel for menu list */
    lv_obj_t             *settings_right_panel; /* Right panel for stats */
    lv_obj_t             *settings_menu_list; /* Menu list in left panel */
    lv_obj_t             *settings_stats_label; /* Stats label in right panel */
    int                   settings_selected_idx; /* Currently selected menu item index */

    /* Font selection page */
    lv_obj_t             *font_selection_overlay; /* Font selection overlay */
    lv_obj_t             *font_selection_list; /* Font list */
    int                   font_selection_selected_idx; /* Currently selected font index */
    char                **font_list; /* Array of font filenames */
    int                   font_list_count; /* Number of fonts found */

    bool                  is_epub;         /* True if EPUB, false if TXT */
} ui_ebook_reader_t;

/**
 * @brief Initialize ebook reader
 * 
 * ⚠️ WARNING: This is a TIME-CONSUMING operation!
 * - For EPUB files: Extracts ZIP, parses metadata, loads content (1-5 seconds)
 * - For TXT files: Loads and parses file (faster, but still I/O bound)
 * 
 * 🔒 THREAD SAFETY:
 * - MUST release LVGL lock (lv_unlock) before calling from LVGL event handlers
 * - Re-acquire lock (lv_lock) after this function returns
 * - Or better: Execute in a separate thread with proper synchronization
 * 
 * @param filepath Path to EPUB or TXT file
 * @param container Container object to display ebook
 * @return Reader handle, NULL on error
 */
ui_ebook_reader_t *ui_ebook_reader_init(const char *filepath, lv_obj_t *container);

/**
 * @brief Cleanup ebook reader
 * @param reader Reader handle
 */
void ui_ebook_reader_cleanup(ui_ebook_reader_t *reader);

/**
 * @brief Display current page
 * @param reader Reader handle
 */
void ui_ebook_reader_display_page(ui_ebook_reader_t *reader);

/**
 * @brief Navigate to next page
 * @param reader Reader handle
 * @return true if successful, false if at end
 */
bool ui_ebook_reader_next_page(ui_ebook_reader_t *reader);

/**
 * @brief Navigate to previous page
 * @param reader Reader handle
 * @return true if successful, false if at beginning
 */
bool ui_ebook_reader_prev_page(ui_ebook_reader_t *reader);

/**
 * @brief Navigate to next chapter
 * @param reader Reader handle
 * @return true if successful, false if at last chapter
 */
bool ui_ebook_reader_next_chapter(ui_ebook_reader_t *reader);

/**
 * @brief Navigate to previous chapter
 * @param reader Reader handle
 * @return true if successful, false if at first chapter
 */
bool ui_ebook_reader_prev_chapter(ui_ebook_reader_t *reader);

/**
 * @brief Increase font size
 * @param reader Reader handle
 */
void ui_ebook_reader_increase_font(ui_ebook_reader_t *reader);

/**
 * @brief Decrease font size
 * @param reader Reader handle
 */
void ui_ebook_reader_decrease_font(ui_ebook_reader_t *reader);

/**
 * @brief Set line spacing
 * @param reader Reader handle
 * @param spacing Line spacing mode (tight, mid, loose)
 */
void ui_ebook_reader_set_line_spacing(ui_ebook_reader_t *reader, ui_ebook_line_spacing_t spacing);

/**
 * @brief Set letter spacing
 * @param reader Reader handle
 * @param spacing Letter spacing in pixels
 */
void ui_ebook_reader_set_letter_spacing(ui_ebook_reader_t *reader, int spacing);

/**
 * @brief Jump to specific page
 * @param reader Reader handle
 * @param page Page number (1-based)
 */
void ui_ebook_reader_jump_to_page(ui_ebook_reader_t *reader, int page);

/**
 * @brief Get current page number
 * @param reader Reader handle
 * @return Current page number (1-based)
 */
int ui_ebook_reader_get_current_page(ui_ebook_reader_t *reader);

/**
 * @brief Get total pages
 * @param reader Reader handle
 * @return Total page count
 */
int ui_ebook_reader_get_total_pages(ui_ebook_reader_t *reader);

/**
 * @brief Handle keyboard input
 * @param reader Reader handle
 * @param key Key code
 */
void ui_ebook_reader_handle_key(ui_ebook_reader_t *reader, lv_key_t key);

/**
 * @brief Show font selection page
 * @param reader Reader handle
 */
void ui_ebook_reader_show_font_selection(ui_ebook_reader_t *reader);

/**
 * @brief Hide font selection page
 * @param reader Reader handle
 */
void ui_ebook_reader_hide_font_selection(ui_ebook_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif /* UI_EBOOK_READER_H */
