import os
import sys

def generate_c_header(mp3_files):
    """生成 media_src.h 头文件内容"""
    header_content = """#ifndef __MEDIA_SRC_H__
#define __MEDIA_SRC_H__

#include "tuya_cloud_types.h"

"""
    for filename, data_length in mp3_files:
        array_name = f"media_src_{filename.replace('-', '_').replace('.', '_')}"
        header_content += f"extern CONST BYTE_T {array_name}[{data_length}];\n"

    header_content += "\n#endif // __MEDIA_SRC_H__\n"
    return header_content


def generate_c_source(mp3_files):
    """生成 media_src.c 源文件内容"""
    source_content = '#include "media_src.h"\n\n'

    for filename, data in mp3_files:
        array_name = f"media_src_{filename.replace('-', '_').replace('.', '_')}"
        hex_array = ', '.join(f'0x{byte:02X}' for byte in data)

        source_content += f"// {filename}\n"
        source_content += f"CONST BYTE_T {array_name}[{len(data)}] = {{\n"
        source_content += f"    {hex_array}\n"
        source_content += "};\n\n"

    return source_content


def convert_mp3_to_c(directory):
    """遍历目录下所有 MP3 文件，统一生成 src/media_src.h 和 media_src.c"""

    mp3_files = []

    for filename in sorted(os.listdir(directory)):
        if filename.lower().endswith(".mp3"):
            filepath = os.path.join(directory, filename)
            with open(filepath, "rb") as f:
                data = f.read()

            mp3_files.append((filename, data))

    if not mp3_files:
        print("No MP3 files found.")
        return

    # 目标目录：../src
    src_dir = os.path.join(directory, "../src")
    os.makedirs(src_dir, exist_ok=True)

    h_file = os.path.join(src_dir, "media_src.h")
    c_file = os.path.join(src_dir, "media_src.c")

    with open(h_file, "w") as h:
        h.write(generate_c_header([(f[0], len(f[1])) for f in mp3_files]))

    with open(c_file, "w") as c:
        c.write(generate_c_source(mp3_files))

    print(f"Generated: {h_file}")
    print(f"Generated: {c_file}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <mp3_directory>")
    else:
        convert_mp3_to_c(sys.argv[1])