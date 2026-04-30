#!/usr/bin/env python3
"""
Image to Monochrome Converter Tool
Converts PNG, JPEG, GIF images to 1-bit and 4-bit grayscale JPEG images
with support for resolution resampling and multiple dithering algorithms.
"""

import argparse
import os
import sys
from pathlib import Path
from PIL import Image, ImageEnhance
import numpy as np


class DitheringAlgorithms:
    """Collection of dithering algorithms for image conversion."""
    
    @staticmethod
    def none(image):
        """No dithering - simple threshold."""
        return image
    
    @staticmethod
    def floyd_steinberg(image):
        """Floyd-Steinberg dithering algorithm."""
        return image.convert('1', dither=Image.Dither.FLOYDSTEINBERG)
    
    @staticmethod
    def ordered_4x4(image):
        """4x4 ordered dithering (Bayer-like)."""
        return image.convert('1', dither=Image.Dither.ORDERED)
    
    @staticmethod
    def threshold(image):
        """Simple threshold dithering."""
        return image.convert('1', dither=Image.Dither.NONE)
    
    @staticmethod
    def custom_bayer_4(image):
        """Custom 4-level Bayer dithering."""
        img_array = np.array(image.convert('L'))
        bayer_matrix_4 = np.array([
            [0, 8, 2, 10],
            [12, 4, 14, 6],
            [3, 11, 1, 9],
            [15, 7, 13, 5]
        ]) * (255 / 16)
        
        h, w = img_array.shape
        dithered = np.zeros_like(img_array)
        
        for y in range(h):
            for x in range(w):
                threshold = bayer_matrix_4[y % 4, x % 4]
                dithered[y, x] = 255 if img_array[y, x] > threshold else 0
        
        return Image.fromarray(dithered.astype('uint8'), mode='L')
    
    @staticmethod
    def custom_bayer_8(image):
        """Custom 8x8 Bayer dithering."""
        img_array = np.array(image.convert('L'))
        bayer_matrix_8 = np.array([
            [0, 32, 8, 40, 2, 34, 10, 42],
            [48, 16, 56, 24, 50, 18, 58, 26],
            [12, 44, 4, 36, 14, 46, 6, 38],
            [60, 28, 52, 20, 62, 30, 54, 22],
            [3, 35, 11, 43, 1, 33, 9, 41],
            [51, 19, 59, 27, 49, 17, 57, 25],
            [15, 47, 7, 39, 13, 45, 5, 37],
            [63, 31, 55, 23, 61, 29, 53, 21]
        ]) * (255 / 64)
        
        h, w = img_array.shape
        dithered = np.zeros_like(img_array)
        
        for y in range(h):
            for x in range(w):
                threshold = bayer_matrix_8[y % 8, x % 8]
                dithered[y, x] = 255 if img_array[y, x] > threshold else 0
        
        return Image.fromarray(dithered.astype('uint8'), mode='L')
    
    @staticmethod
    def custom_bayer_16(image):
        """Custom 16x16 Bayer dithering."""
        img_array = np.array(image.convert('L'))
        # Generate 16x16 Bayer matrix
        bayer_16 = np.zeros((16, 16))
        for i in range(16):
            for j in range(16):
                bayer_16[i, j] = (i * 16 + j) * (255 / 256)
        
        h, w = img_array.shape
        dithered = np.zeros_like(img_array)
        
        for y in range(h):
            for x in range(w):
                threshold = bayer_16[y % 16, x % 16]
                dithered[y, x] = 255 if img_array[y, x] > threshold else 0
        
        return Image.fromarray(dithered.astype('uint8'), mode='L')
    
    @staticmethod
    def atkinson(image):
        """Atkinson dithering algorithm (custom implementation)."""
        img_array = np.array(image.convert('L')).astype(np.float32)
        h, w = img_array.shape
        output = np.zeros_like(img_array)
        error = np.zeros_like(img_array)
        
        for y in range(h):
            for x in range(w):
                old_pixel = img_array[y, x] + error[y, x]
                new_pixel = 255 if old_pixel > 127 else 0
                output[y, x] = new_pixel
                quant_error = old_pixel - new_pixel
                
                # Distribute error to neighbors
                if x + 1 < w:
                    error[y, x + 1] += quant_error * 1/8
                if x + 2 < w:
                    error[y, x + 2] += quant_error * 1/8
                if y + 1 < h:
                    if x - 1 >= 0:
                        error[y + 1, x - 1] += quant_error * 1/8
                    error[y + 1, x] += quant_error * 1/8
                    if x + 1 < w:
                        error[y + 1, x + 1] += quant_error * 1/8
                if y + 2 < h:
                    error[y + 2, x] += quant_error * 1/8
        
        return Image.fromarray(np.clip(output, 0, 255).astype('uint8'), mode='L')


class ImageConverter:
    """Main image conversion class."""
    
    DITHERING_METHODS = {
        'none': DitheringAlgorithms.none,
        'threshold': DitheringAlgorithms.threshold,
        'floyd-steinberg': DitheringAlgorithms.floyd_steinberg,
        'ordered-4x4': DitheringAlgorithms.ordered_4x4,
        'bayer-4': DitheringAlgorithms.custom_bayer_4,
        'bayer-8': DitheringAlgorithms.custom_bayer_8,
        'bayer-16': DitheringAlgorithms.custom_bayer_16,
        'atkinson': DitheringAlgorithms.atkinson,
    }
    
    def __init__(self, input_path, output_dir=None, width=None, height=None, 
                 dither='floyd-steinberg', quality=95, contrast=1.0):
        """
        Initialize the converter.
        
        Args:
            input_path: Path to input image file
            output_dir: Output directory (default: same as input)
            width: Target width (None to maintain aspect ratio)
            height: Target height (None to maintain aspect ratio)
            dither: Dithering algorithm to use
            quality: JPEG quality (1-100)
            contrast: Contrast enhancement factor (1.0 = no change, >1.0 = more contrast)
        """
        self.input_path = Path(input_path)
        self.output_dir = Path(output_dir) if output_dir else self.input_path.parent
        self.width = width
        self.height = height
        self.dither_method = dither
        self.quality = quality
        self.contrast = contrast
        
        if not self.input_path.exists():
            raise FileNotFoundError(f"Input file not found: {input_path}")
        
        if dither not in self.DITHERING_METHODS:
            raise ValueError(f"Unknown dithering method: {dither}. "
                           f"Available: {', '.join(self.DITHERING_METHODS.keys())}")
    
    def load_image(self):
        """Load and preprocess the input image."""
        try:
            img = Image.open(self.input_path)
            # Convert to RGB if necessary (handles RGBA, P, etc.)
            if img.mode != 'RGB':
                rgb_img = Image.new('RGB', img.size)
                if img.mode == 'RGBA':
                    rgb_img.paste(img, mask=img.split()[3])
                else:
                    rgb_img.paste(img)
                img = rgb_img
            return img
        except Exception as e:
            raise ValueError(f"Failed to load image: {e}")
    
    def resize_image(self, image):
        """Resize image if dimensions are specified."""
        if self.width or self.height:
            # Calculate dimensions maintaining aspect ratio
            original_width, original_height = image.size
            
            if self.width and self.height:
                # Both specified - use exact dimensions
                new_size = (self.width, self.height)
            elif self.width:
                # Only width specified
                ratio = self.width / original_width
                new_size = (self.width, int(original_height * ratio))
            else:
                # Only height specified
                ratio = self.height / original_height
                new_size = (int(original_width * ratio), self.height)
            
            # Use high-quality resampling
            return image.resize(new_size, Image.Resampling.LANCZOS)
        return image
    
    def convert_to_grayscale(self, image):
        """Convert RGB image to grayscale."""
        grayscale = image.convert('L')
        
        # Apply contrast enhancement if specified
        if self.contrast != 1.0:
            enhancer = ImageEnhance.Contrast(grayscale)
            grayscale = enhancer.enhance(self.contrast)
        
        return grayscale
    
    def apply_dithering(self, grayscale_image):
        """Apply the selected dithering algorithm."""
        dither_func = self.DITHERING_METHODS[self.dither_method]
        return dither_func(grayscale_image)
    
    def convert_to_1bit(self, grayscale_image):
        """Convert grayscale image to 1-bit (monochrome)."""
        if self.dither_method == 'none':
            # Simple threshold
            return grayscale_image.point(lambda x: 255 if x > 127 else 0, mode='1')
        elif self.dither_method in ['floyd-steinberg', 'ordered-4x4', 'threshold']:
            # PIL built-in dithering
            if self.dither_method == 'floyd-steinberg':
                return grayscale_image.convert('1', dither=Image.Dither.FLOYDSTEINBERG)
            elif self.dither_method == 'ordered-4x4':
                return grayscale_image.convert('1', dither=Image.Dither.ORDERED)
            else:
                return grayscale_image.convert('1', dither=Image.Dither.NONE)
        else:
            # Custom dithering algorithms return already dithered grayscale
            dithered = self.apply_dithering(grayscale_image)
            return dithered.convert('1')
    
    def convert_to_4bit(self, grayscale_image):
        """Convert grayscale image to 4-bit (16 levels)."""
        # Quantize to 16 levels
        quantized = grayscale_image.point(lambda x: int(x / 17) * 17)
        return quantized
    
    def save_jpeg(self, image, output_path, mode='L'):
        """Save image as JPEG in the specified mode."""
        # Convert to appropriate mode for JPEG
        if mode == '1':
            # Convert 1-bit to grayscale for JPEG
            jpeg_img = image.convert('L')
        elif mode == '4bit':
            # 4-bit grayscale (16 levels)
            jpeg_img = image.convert('L')
        else:
            jpeg_img = image.convert('L')
        
        jpeg_img.save(output_path, 'JPEG', quality=self.quality, optimize=True)
    
    def convert(self):
        """Perform the complete conversion process."""
        # Load image
        print(f"Loading image: {self.input_path}")
        image = self.load_image()
        
        # Resize if needed
        if self.width or self.height:
            print(f"Resizing to: {self.width or 'auto'}x{self.height or 'auto'}")
            image = self.resize_image(image)
        
        # Convert to grayscale
        print("Converting to grayscale...")
        grayscale = self.convert_to_grayscale(image)
        if self.contrast != 1.0:
            print(f"Applying contrast enhancement (factor: {self.contrast})...")
        
        # Generate output filenames
        base_name = self.input_path.stem
        output_1bit = self.output_dir / f"{base_name}_1bit_{self.dither_method}.jpg"
        output_4bit = self.output_dir / f"{base_name}_4bit_{self.dither_method}.jpg"
        
        # Convert to 1-bit
        print(f"Converting to 1-bit monochrome with {self.dither_method} dithering...")
        mono_1bit = self.convert_to_1bit(grayscale)
        self.save_jpeg(mono_1bit, output_1bit, mode='1')
        print(f"Saved 1-bit image: {output_1bit}")
        
        # Convert to 4-bit
        print(f"Converting to 4-bit grayscale with {self.dither_method} dithering...")
        mono_4bit = self.convert_to_4bit(grayscale)
        if self.dither_method != 'none':
            # Apply dithering for 4-bit as well
            mono_4bit = self.apply_dithering(mono_4bit)
        self.save_jpeg(mono_4bit, output_4bit, mode='4bit')
        print(f"Saved 4-bit image: {output_4bit}")
        
        return output_1bit, output_4bit


def main():
    """Main entry point for command-line interface."""
    parser = argparse.ArgumentParser(
        description='Convert images to 1-bit and 4-bit grayscale JPEG images',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic conversion
  python img2mono.py input.png
  
  # With resizing and custom dithering
  python img2mono.py input.jpg -w 800 -h 600 -d bayer-8
  
  # Batch convert all images in a directory
  python img2mono.py *.png -o output_dir -d floyd-steinberg
  
  # Convert with all algorithms and increased contrast
  python img2mono.py input.png --all-algorithms -c 1.5
  
  # Single algorithm with contrast enhancement
  python img2mono.py input.png -d bayer-8 -c 2.0
  
Available dithering algorithms:
  none              - No dithering (simple threshold)
  threshold         - Simple threshold
  floyd-steinberg   - Floyd-Steinberg dithering (default)
  ordered-4x4       - 4x4 ordered dithering
  bayer-4           - Custom 4x4 Bayer dithering
  bayer-8           - Custom 8x8 Bayer dithering
  bayer-16          - Custom 16x16 Bayer dithering
  atkinson          - Atkinson dithering algorithm
        """
    )
    
    parser.add_argument('input', nargs='+', help='Input image file(s) (PNG, JPEG, GIF)')
    parser.add_argument('-o', '--output', dest='output_dir', 
                       help='Output directory (default: same as input)')
    parser.add_argument('-w', '--width', type=int, 
                       help='Target width (maintains aspect ratio if height not specified)')
    parser.add_argument('--height', type=int, 
                       help='Target height (maintains aspect ratio if width not specified)')
    parser.add_argument('-d', '--dither', default='floyd-steinberg',
                       choices=list(ImageConverter.DITHERING_METHODS.keys()),
                       help='Dithering algorithm to use (default: floyd-steinberg)')
    parser.add_argument('-q', '--quality', type=int, default=95, choices=range(1, 101),
                       metavar='1-100',
                       help='JPEG quality (1-100, default: 95)')
    parser.add_argument('-c', '--contrast', type=float, default=1.0,
                       help='Contrast enhancement factor (1.0 = no change, >1.0 = more contrast, default: 1.0)')
    parser.add_argument('--all-algorithms', action='store_true',
                       help='Convert with all dithering algorithms')
    
    args = parser.parse_args()
    
    # Determine which algorithms to use
    if args.all_algorithms:
        algorithms = list(ImageConverter.DITHERING_METHODS.keys())
    else:
        algorithms = [args.dither]
    
    # Process each input file
    for input_path in args.input:
        try:
            # Convert with each algorithm
            for dither_method in algorithms:
                converter = ImageConverter(
                    input_path=input_path,
                    output_dir=args.output_dir,
                    width=args.width,
                    height=args.height,
                    dither=dither_method,
                    quality=args.quality,
                    contrast=args.contrast
                )
                converter.convert()
                print()
        except Exception as e:
            print(f"Error processing {input_path}: {e}", file=sys.stderr)
            continue
    
    print("Conversion complete!")


if __name__ == '__main__':
    main()

