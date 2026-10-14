#!/usr/bin/env python3
"""Generate original geometric RGB565 avatars; no personal image or dependency."""
from pathlib import Path

def pixels(width, height):
    data = bytearray()
    for y in range(height):
        for x in range(width):
            color = 0x18e3
            if width // 5 < x < 4 * width // 5 and height // 4 < y < 3 * height // 4:
                color = 0x07ff
            if y in range(height // 2 - 2, height // 2 + 2) and (
                x in range(width // 3 - 2, width // 3 + 2)
                or x in range(2 * width // 3 - 2, 2 * width // 3 + 2)
            ):
                color = 0x18e3
            data.extend((color & 255, color >> 8))
    return data

def image(symbol, width, height):
    data = pixels(width, height)
    rows = ["    " + ", ".join(f"0x{b:02x}" for b in data[i:i+16]) + ","
            for i in range(0, len(data), 16)]
    return (f"static const uint8_t {symbol}_map[] = {{\n" + "\n".join(rows) + "\n};\n"
        + f"\nconst lv_image_dsc_t {symbol}_image = {{\n"
        + "    .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
        + "    .header.cf = LV_COLOR_FORMAT_RGB565,\n"
        + "    .header.flags = 0,\n"
        + f"    .header.w = {width},\n    .header.h = {height},\n"
        + f"    .header.stride = {width * 2},\n"
        + f"    .data_size = sizeof({symbol}_map),\n    .data = {symbol}_map,\n}};\n")

root = Path(__file__).resolve().parents[1]
(root / "main/profile_images.c").write_text(
    '#include "profile_images.h"\n#include <stdint.h>\n'
    + image("profile_avatar", 72, 96) + image("quest_card_avatar", 42, 56)
)
