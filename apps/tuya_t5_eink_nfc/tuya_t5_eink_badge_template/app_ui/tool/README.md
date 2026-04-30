# TTF to LVGL Binary Font Converter

A Python script to convert TrueType Font (TTF) files to LVGL-compatible binary format for dynamic loading from SD card without loading the entire font into RAM.

## Features

- **Complete Font Support**: By default, includes all characters from the source TTF file (no range restrictions)
- **Dynamic Font Loading**: Binary fonts can be loaded on-demand, reading only needed glyphs from the file
- **Minimal File Size**: Optionally limit to specific Unicode ranges for smaller file sizes
- **Memory Efficient**: LVGL's binary font loader reads glyphs from file as needed, not loading the entire font into RAM
- **Flexible Options**: Support for different font sizes, bit depths, compression, and LCD filtering
- **No Installation Required**: Automatically uses `npx lv_font_conv` if not installed globally

## Requirements

- Python 3.6+
- Node.js and npm (for `lv_font_conv`)
  - The script will automatically use `npx lv_font_conv` if available
  - Or install globally: `npm install -g lv_font_conv`

## Installation

No installation needed! The script uses `npx` to run `lv_font_conv` automatically. Just ensure you have Node.js installed.

If you prefer to install `lv_font_conv` globally:

```bash
npm install -g lv_font_conv
```

## Usage

### Basic Usage

```bash
python3 tff2bin.py <input_ttf> <output_bin> [options]
```

### Example: Convert TTF to Binary Font (Complete Range - Default)

```bash
# Include ALL characters from the source TTF file (default behavior)
# No --range specified = complete font with all available characters
# The complete range of whatever the source TTF provides
python3 tff2bin.py ./LXGWWenKaiMono-Regular.ttf font.bin --size 24 --bpp 1 --no-compress
```

**Default Behavior**: When no `--range` or `--range-set` is specified, the script includes the complete range of all characters available in the source TTF file. Every glyph, character, and symbol that exists in the TTF will be included in the binary output.

### Example: Convert TTF to Binary Font (Limited Range)

```bash
# Include only specific Unicode ranges for smaller file size
python3 tff2bin.py ./LXGWWenKaiMono-Regular.ttf font.bin --size 24 --bpp 1 --range "0x20-0x7F,0x3000-0x303F,0x4E00-0x9FA5" --no-compress
```

This limited range command:
- Converts `LXGWWenKaiMono-Regular.ttf` to `font.bin`
- Sets font size to 24 pixels
- Uses 1 bpp (monochrome) for minimal file size
- Includes only:
  - ASCII characters (0x20-0x7F)
  - CJK Symbols and Punctuation (0x3000-0x303F)
  - CJK Unified Ideographs (0x4E00-0x9FA5)

## Command-Line Options

### Required Arguments

- `input_ttf`: Path to input TTF file
- `output_bin`: Path to output binary file

### Optional Arguments

| Option | Description | Default |
|--------|-------------|---------|
| `--size SIZE` | Font size in pixels | 16 |
| `--bpp {1,2,3,4,8}` | Bits per pixel: 1=monochrome, 2=4-level, 3=8-level, 4=16-level, 8=256-level | 4 |
| `--range RANGE` | Unicode range(s) to include (comma-separated) | All characters (no limit) |
| `--range-set SET` | Use predefined range set (see below) | - |
| `--symbols SYMBOLS` | Additional symbols to include (e.g., "©®™€£¥") | - |
| `--no-compress` | Disable compression (may increase file size) | Compression enabled |
| `--no-prefilter` | Disable prefiltering | Prefiltering enabled |
| `--lcd` | Enable LCD subpixel filtering (horizontal) | Disabled |
| `--lcd-v` | Enable LCD subpixel filtering (vertical) | Disabled |
| `--install-tool` | Install lv_font_conv globally if not found | - |

### Predefined Range Sets

Use `--range-set` for common character sets:

- `ascii`: Basic ASCII (0x20-0x7F)
- `latin1`: Latin-1 Supplement (0x20-0xFF)
- `latin-ext`: Latin Extended (0x20-0x24F)
- `cjk`: CJK Unified Ideographs (0x4E00-0x9FFF)
- `cjk-ext`: Extended CJK ranges
- `common`: Common European characters

## Usage Examples

### 1. Basic ASCII Font (Minimal Size)

```bash
python3 tff2bin.py font.ttf font.bin --size 16 --bpp 1 --range "0x20-0x7F"
```

### 3. Chinese/Japanese/Korean Font

```bash
python3 tff2bin.py font.ttf font.bin --size 24 --bpp 4 --range-set cjk
```

Or with custom ranges:

```bash
python3 tff2bin.py font.ttf font.bin --size 24 --bpp 1 --range "0x20-0x7F,0x3000-0x303F,0x4E00-0x9FA5"
```

### 4. High-Quality Font with LCD Filtering

```bash
python3 tff2bin.py font.ttf font.bin --size 18 --bpp 4 --lcd --range "0x20-0xFF"
```

### 5. Include Specific Symbols

```bash
python3 tff2bin.py font.ttf font.bin --size 16 --symbols "©®™€£¥"
```

### 6. Combine Ranges and Symbols

```bash
python3 tff2bin.py font.ttf font.bin --size 16 --range "0x20-0x7F" --symbols "©®™"
```

## Unicode Range Format

The `--range` option accepts comma-separated Unicode ranges in various formats:

- Single range: `"0x20-0x7F"`
- Multiple ranges: `"0x20-0x7F,0xA0-0xFF"`
- Decimal format: `"32-127"`
- Mixed format: `"0x20-0x7F,0x4E00-0x9FA5"`

### Common Unicode Ranges

| Range | Description |
|-------|-------------|
| `0x20-0x7F` | Basic ASCII printable characters |
| `0xA0-0xFF` | Latin-1 Supplement |
| `0x100-0x17F` | Latin Extended-A |
| `0x4E00-0x9FA5` | CJK Unified Ideographs (Chinese, Japanese, Korean) |
| `0x3000-0x303F` | CJK Symbols and Punctuation |
| `0x3040-0x309F` | Hiragana (Japanese) |
| `0x30A0-0x30FF` | Katakana (Japanese) |
| `0xAC00-0xD7AF` | Hangul Syllables (Korean) |

## Loading Fonts in LVGL

After conversion, load the binary font dynamically in your LVGL code:

```c
#include "lvgl.h"

// Load font from SD card
lv_font_t *font = lv_binfont_create("S:/path/to/font.bin");
if (font) {
    // Use the font
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);

    // ... your code ...

    // Clean up when done (optional, fonts are cached)
    // lv_binfont_destroy(font);
}
```

### Important Notes

- Ensure LVGL filesystem is configured to access the SD card
- The font path should match your filesystem mount point (e.g., `"S:/fonts/font.bin"`)
- Fonts are loaded on-demand, so only glyphs actually used are read from the file
- Binary fonts support partial loading, minimizing RAM usage

## Minimizing File Size

To keep binary font files as small as possible:

1. **Use lower BPP**: `--bpp 1` (monochrome) is smallest, `--bpp 4` is a good balance
2. **Include only needed ranges**: Use `--range` to specify only required Unicode ranges
3. **Enable compression**: Keep compression enabled (default)
4. **Use prefiltering**: Keep prefiltering enabled (default) for better compression

### File Size Comparison Example

For a CJK font at 24px:
- `--bpp 8 --range-set cjk`: ~2-5 MB
- `--bpp 4 --range-set cjk`: ~1-2 MB
- `--bpp 1 --range "0x20-0x7F,0x4E00-0x9FA5"`: ~200-500 KB

## Troubleshooting

### Error: lv_font_conv not found

The script will automatically try to use `npx lv_font_conv`. If that fails:

1. Install Node.js and npm
2. Run: `npm install -g lv_font_conv`
3. Or use the `--install-tool` flag

### Large File Sizes

- Reduce BPP (use `--bpp 1` for monochrome)
- Limit Unicode ranges to only what you need
- Ensure compression is enabled (don't use `--no-compress`)

### Font Not Loading in LVGL

- Verify the file path is correct
- Ensure LVGL filesystem is properly configured
- Check that the binary file was created successfully
- Verify LVGL binary font support is enabled in your configuration

## Technical Details

### Binary Format

The script uses LVGL's binary font format, which:
- Stores font metadata and glyph bitmaps in a compact binary format
- Supports random access to individual glyphs
- Enables on-demand loading without reading the entire file
- Uses RLE compression for bitmap data (when enabled)

### Memory Usage

- **Static fonts**: Entire font loaded into RAM
- **Binary fonts**: Only glyphs in use are loaded, significantly reducing RAM usage
- **Glyph cache**: LVGL caches recently used glyphs for performance

## License

This script is provided as-is for use with LVGL font conversion.

## See Also

- [LVGL Documentation](https://docs.lvgl.io/)
- [lv_font_conv GitHub](https://github.com/lvgl/lv_font_conv)
- [LVGL Binary Font Loader](https://docs.lvgl.io/master/main-modules/fonts/binfont_loader.html)
