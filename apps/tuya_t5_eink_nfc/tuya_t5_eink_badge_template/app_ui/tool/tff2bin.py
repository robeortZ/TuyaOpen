#!/usr/bin/env python3
"""
TTF to LVGL Binary Font Converter

This script converts TrueType Font (TTF) files to LVGL-compatible binary format
that can be loaded dynamically from SD card without loading the entire file into RAM.

The binary format supports:
- Partial glyph loading (only loads needed glyphs)
- Minimal file size (only includes specified Unicode ranges)
- Dynamic font loading via lv_binfont_create()
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path


def get_lv_font_conv_cmd():
    """Get the command to run lv_font_conv (try npx first, then direct)."""
    # Try direct command first (if installed globally - faster)
    try:
        result = subprocess.run(
            ['lv_font_conv', '--version'],
            capture_output=True,
            text=True,
            check=True,
            timeout=5
        )
        return ['lv_font_conv']
    except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
        # Try npx (works without global installation)
        try:
            result = subprocess.run(
                ['npx', 'lv_font_conv', '--version'],
                capture_output=True,
                text=True,
                check=True,
                timeout=10
            )
            return ['npx', 'lv_font_conv']
        except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
            return None


def check_lv_font_conv():
    """Check if lv_font_conv is available (via npx or direct)."""
    return get_lv_font_conv_cmd() is not None


def install_lv_font_conv():
    """Attempt to install lv_font_conv via npm."""
    print("lv_font_conv not found. Attempting to install via npm...")
    try:
        subprocess.run(
            ['npm', 'install', '-g', 'lv_font_conv'],
            check=True
        )
        print("Successfully installed lv_font_conv")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("ERROR: npm not found. Please install Node.js and npm first.")
        print("Then run: npm install -g lv_font_conv")
        return False


def parse_unicode_ranges(range_str):
    """
    Parse Unicode range string into list of ranges.

    Examples:
        "0x20-0x7F" -> [(32, 127)]
        "0x20-0x7F,0xA0-0xFF" -> [(32, 127), (160, 255)]
        "0x4E00-0x9FFF" -> [(19968, 40959)]  # CJK Unified Ideographs
    """
    ranges = []
    for part in range_str.split(','):
        part = part.strip()
        if '-' in part:
            start_str, end_str = part.split('-', 1)
            start = int(start_str.strip(), 0)  # Supports 0x, 0X, or decimal
            end = int(end_str.strip(), 0)
            ranges.append((start, end))
        else:
            # Single character
            val = int(part.strip(), 0)
            ranges.append((val, val))
    return ranges


def build_range_args(ranges):
    """
    Build --range arguments for lv_font_conv from parsed ranges.

    Note: lv_font_conv supports comma-separated ranges in a single --range argument,
    so we can pass them directly. This function is kept for backward compatibility
    but the main code now passes ranges directly.
    """
    args = []
    range_parts = []
    for start, end in ranges:
        if start == end:
            range_parts.append(f'0x{start:X}')
        else:
            range_parts.append(f'0x{start:X}-0x{end:X}')
    # Join with comma and pass as single --range argument
    if range_parts:
        args.extend(['--range', ','.join(range_parts)])
    return args


def convert_ttf_to_bin(
    input_ttf,
    output_bin,
    size=16,
    bpp=4,
    unicode_ranges=None,
    symbols=None,
    compress=True,
    prefilter=True,
    lcd_filter=False,
    lcd_vertical=False
):
    """
    Convert TTF file to LVGL binary format.

    Args:
        input_ttf: Path to input TTF file
        output_bin: Path to output binary file
        size: Font size in pixels (default: 16)
        bpp: Bits per pixel - 1, 2, 3, 4, or 8 (default: 4)
        unicode_ranges: Unicode range string (e.g., "0x20-0x7F,0xA0-0xFF")
        symbols: Additional symbols to include (string of characters)
        compress: Enable compression (default: True)
        prefilter: Enable prefiltering (default: True)
        lcd_filter: Enable LCD subpixel filtering (horizontal, default: False)
        lcd_vertical: Enable LCD subpixel filtering (vertical, default: False)
    """
    if not os.path.exists(input_ttf):
        print(f"ERROR: Input file not found: {input_ttf}")
        return False

    # Get the command to use (npx or direct)
    lv_font_conv_cmd = get_lv_font_conv_cmd()
    if not lv_font_conv_cmd:
        print("ERROR: lv_font_conv not available")
        return False

    # Build command
    cmd = lv_font_conv_cmd + [
        '--font', input_ttf,
        '--size', str(size),
        '--bpp', str(bpp),
        '--format', 'bin',
        '-o', output_bin
    ]

    # Add Unicode ranges
    # lv_font_conv supports comma-separated ranges in a single --range argument
    # Examples: "0x20-0x7F,0x3000-0x303F,0x4E00-0x9FA5" or "32-127,0x1F450"
    if unicode_ranges:
        # Pass the comma-separated string directly to lv_font_conv
        # It natively supports comma-separated ranges
        cmd.extend(['--range', unicode_ranges])

    # Add symbols
    if symbols:
        cmd.extend(['--symbols', symbols])

    # Compression options
    if not compress:
        cmd.append('--no-compress')

    if not prefilter:
        cmd.append('--no-prefilter')

    # LCD filtering (horizontal or vertical)
    if lcd_vertical:
        cmd.append('--lcd-v')
    elif lcd_filter:
        cmd.append('--lcd')

    print(f"Converting {input_ttf} to {output_bin}...")
    print(f"  Size: {size}px, BPP: {bpp}")
    if unicode_ranges:
        print(f"  Unicode ranges: {unicode_ranges}")
    if symbols:
        print(f"  Additional symbols: {len(symbols)} characters")

    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        if result.stdout:
            print(result.stdout)

        # Check output file size
        if os.path.exists(output_bin):
            file_size = os.path.getsize(output_bin)
            print(f"\n✓ Conversion successful!")
            print(f"  Output file: {output_bin}")
            print(f"  File size: {file_size:,} bytes ({file_size / 1024:.2f} KB)")
            return True
        else:
            print("ERROR: Output file was not created")
            return False

    except subprocess.CalledProcessError as e:
        print(f"ERROR: Conversion failed")
        if e.stderr:
            print(f"Error output: {e.stderr}")
        if e.stdout:
            print(f"Standard output: {e.stdout}")
        return False


def get_default_ranges():
    """Get default Unicode ranges for common character sets."""
    return {
        'ascii': '0x20-0x7F',  # Basic ASCII printable
        'latin1': '0x20-0xFF',  # Latin-1 Supplement
        'latin-ext': '0x20-0x24F',  # Latin Extended
        'cjk': '0x4E00-0x9FFF',  # CJK Unified Ideographs
        'cjk-ext': '0x4E00-0x9FFF,0x3400-0x4DBF,0x20000-0x2A6DF',  # Extended CJK
        'common': '0x20-0x7F,0xA0-0xFF,0x100-0x17F',  # Common European
    }


def main():
    parser = argparse.ArgumentParser(
        description='Convert TTF font to LVGL binary format for dynamic loading',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic conversion (includes all characters from font)
  %(prog)s font.ttf font.bin --size 16

  # Include only specific Unicode ranges (for smaller file size)
  %(prog)s font.ttf font.bin --size 16 --range "0x20-0xFF,0x4E00-0x9FFF"

  # Use predefined range set
  %(prog)s font.ttf font.bin --size 16 --range-set cjk

  # Include specific symbols
  %(prog)s font.ttf font.bin --size 16 --symbols "©®™€£¥"

  # Minimize file size with 1 bpp (monochrome)
  %(prog)s font.ttf font.bin --size 16 --bpp 1 --range "0x20-0x7F"

  # Enable LCD subpixel filtering (horizontal)
  %(prog)s font.ttf font.bin --size 16 --lcd

  # Enable LCD subpixel filtering (vertical)
  %(prog)s font.ttf font.bin --size 16 --lcd-v

Available range sets:
  ascii      - Basic ASCII (0x20-0x7F)
  latin1     - Latin-1 Supplement (0x20-0xFF)
  latin-ext  - Latin Extended (0x20-0x24F)
  cjk        - CJK Unified Ideographs (0x4E00-0x9FFF)
  cjk-ext    - Extended CJK ranges
  common     - Common European characters

Note: The binary font can be loaded in LVGL using:
  lv_font_t *font = lv_binfont_create("S:/path/to/font.bin");
        """
    )

    parser.add_argument(
        'input_ttf',
        help='Path to input TTF file'
    )

    parser.add_argument(
        'output_bin',
        help='Path to output binary file'
    )

    parser.add_argument(
        '--size',
        type=int,
        default=16,
        help='Font size in pixels (default: 16)'
    )

    parser.add_argument(
        '--bpp',
        type=int,
        choices=[1, 2, 3, 4, 8],
        default=4,
        help='Bits per pixel: 1=monochrome, 2=4-level, 3=8-level, 4=16-level, 8=256-level (default: 4)'
    )

    parser.add_argument(
        '--range',
        type=str,
        help='Unicode range(s) to include (e.g., "0x20-0x7F" or "0x20-0x7F,0xA0-0xFF"). If not specified, all characters from the font are included.'
    )

    parser.add_argument(
        '--range-set',
        choices=['ascii', 'latin1', 'latin-ext', 'cjk', 'cjk-ext', 'common'],
        help='Use predefined Unicode range set'
    )

    parser.add_argument(
        '--symbols',
        type=str,
        help='Additional symbols to include (e.g., "©®™€£¥")'
    )

    parser.add_argument(
        '--no-compress',
        action='store_true',
        help='Disable compression (may increase file size)'
    )

    parser.add_argument(
        '--no-prefilter',
        action='store_true',
        help='Disable prefiltering'
    )

    parser.add_argument(
        '--lcd',
        action='store_true',
        help='Enable LCD subpixel filtering (horizontal pixel layout)'
    )

    parser.add_argument(
        '--lcd-v',
        action='store_true',
        help='Enable LCD subpixel filtering (vertical pixel layout)'
    )

    parser.add_argument(
        '--install-tool',
        action='store_true',
        help='Install lv_font_conv tool if not found'
    )

    args = parser.parse_args()

    # Check for lv_font_conv
    if not check_lv_font_conv():
        if args.install_tool:
            if not install_lv_font_conv():
                sys.exit(1)
        else:
            print("ERROR: lv_font_conv not found!")
            print("The script will try to use 'npx lv_font_conv' automatically.")
            print("If that fails, install it with: npm install -g lv_font_conv")
            print("Or use --install-tool flag to install globally")
            sys.exit(1)

    # Determine Unicode ranges
    unicode_ranges = args.range
    if args.range_set:
        default_ranges = get_default_ranges()
        unicode_ranges = default_ranges.get(args.range_set)
        if not unicode_ranges:
            print(f"ERROR: Unknown range set: {args.range_set}")
            sys.exit(1)

    # If no range specified, use full Unicode range to include all characters
    # Default: Complete range of whatever the source TTF provides
    # lv_font_conv requires either --range or --symbols, so we use full Unicode range
    if not unicode_ranges and not args.symbols:
        print("No Unicode range specified. Using full Unicode range (0x0-0x10FFFF) to include all characters from the source TTF.")
        print("Use --range or --range-set to limit characters for smaller file size.")
        # Full Unicode range: 0x0 to 0x10FFFF (covers all Unicode characters)
        unicode_ranges = '0x0-0x10FFFF'

    # Convert
    success = convert_ttf_to_bin(
        input_ttf=args.input_ttf,
        output_bin=args.output_bin,
        size=args.size,
        bpp=args.bpp,
        unicode_ranges=unicode_ranges,
        symbols=args.symbols,
        compress=not args.no_compress,
        prefilter=not args.no_prefilter,
        lcd_filter=args.lcd,
        lcd_vertical=args.lcd_v
    )

    if success:
        print("\n" + "="*60)
        print("Usage in LVGL:")
        print("="*60)
        print(f"  lv_font_t *font = lv_binfont_create(\"{args.output_bin}\");")
        print("  if (font) {")
        print("      lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);")
        print("  }")
        print("\nNote: Ensure LVGL filesystem is configured to access the file.")
        print("="*60)
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
