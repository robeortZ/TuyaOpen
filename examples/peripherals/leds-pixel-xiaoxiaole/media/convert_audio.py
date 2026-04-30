#!/usr/bin/env python3
"""Convert MP3 audio files to C header and source files for embedding in firmware."""

import os
import sys

def convert_mp3_to_c(media_dir, output_dir):
    """Convert all MP3 files in media_dir to C arrays in output_dir."""

    # Audio files to process
    audio_files = {
        'kaiji.mp3': 'kaiji',           # Startup sound
        'SHOT.mp3': 'shot_sound',       # Shooting sound
        'error.mp3': 'error_sound',     # Failure sound
        'success.mp3': 'success_sound', # Success sound
    }

    audio_data = {}

    # Read all MP3 files
    for filename, var_name in audio_files.items():
        filepath = os.path.join(media_dir, filename)
        if os.path.exists(filepath):
            with open(filepath, 'rb') as f:
                data = f.read()
            audio_data[var_name] = (filename, data)
            print(f"Read {filename}: {len(data)} bytes")
        else:
            print(f"Warning: {filename} not found")

    if not audio_data:
        print("No audio files found!")
        return

    # Generate media_audio.h
    h_file = os.path.join(output_dir, 'media_audio.h')
    with open(h_file, 'w') as f:
        f.write('#ifndef __MEDIA_AUDIO_H__\n')
        f.write('#define __MEDIA_AUDIO_H__\n\n')
        f.write('#include "tuya_cloud_types.h"\n\n')

        for var_name, (filename, data) in audio_data.items():
            f.write(f'extern CONST BYTE_T {var_name}[{len(data)}];\n')
            f.write(f'#define {var_name.upper()}_SIZE {len(data)}\n\n')

        f.write('#endif // __MEDIA_AUDIO_H__\n')

    print(f"Generated: {h_file}")

    # Generate media_audio.c
    c_file = os.path.join(output_dir, 'media_audio.c')
    with open(c_file, 'w') as f:
        f.write('#include "media_audio.h"\n\n')

        for var_name, (filename, data) in audio_data.items():
            f.write(f'// {filename}\n')
            f.write(f'CONST BYTE_T {var_name}[{len(data)}] = {{\n')

            # Write hex data in lines of 16 bytes
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                hex_bytes = ', '.join(f'0x{b:02X}' for b in chunk)
                f.write(f'    {hex_bytes},\n')

            f.write('};\n\n')

    print(f"Generated: {c_file}")

if __name__ == "__main__":
    media_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(media_dir, '../src')

    print(f"Media directory: {media_dir}")
    print(f"Output directory: {output_dir}")

    convert_mp3_to_c(media_dir, output_dir)
