#!/usr/bin/env python3
"""Generate the RGB565 LVGL avatar for the local AI Passport profile."""

from pathlib import Path
import sys

from PIL import Image


def crop_avatar(image: Image.Image) -> Image.Image:
    """Fit the complete 3:4 portrait into the on-device avatar frame."""
    return image.resize((72, 96), Image.Resampling.LANCZOS)


def rgb565_bytes(image: Image.Image) -> bytes:
    data = bytearray()
    pixels = image.convert("RGB")
    for y in range(pixels.height):
        for x in range(pixels.width):
            red, green, blue = pixels.getpixel((x, y))
            value = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
            data.extend((value & 0xFF, value >> 8))
    return bytes(data)


def array(name: str, data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + 16]) + ",")
    return f"static const uint8_t {name}[] = {{\n" + "\n".join(rows) + "\n};\n"


def descriptor(name: str, data_name: str, width: int, height: int) -> str:
    return f"""
const lv_image_dsc_t {name} = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.flags = 0,
    .header.w = {width},
    .header.h = {height},
    .header.stride = {width * 2},
    .data_size = sizeof({data_name}),
    .data = {data_name},
}};
"""


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_profile_images.py SOURCE_IMAGE OUTPUT_C")
    image = Image.open(sys.argv[1]).convert("RGB")
    avatar = rgb565_bytes(crop_avatar(image))
    output = "#include \"profile_images.h\"\n#include <stdint.h>\n\n"
    output += array("profile_avatar_map", avatar)
    output += descriptor("profile_avatar_image", "profile_avatar_map", 72, 96)
    # The compact quest card must not allocate a runtime transform buffer.
    card = rgb565_bytes(image.resize((42, 56), Image.Resampling.LANCZOS))
    output += array("quest_card_avatar_map", card)
    output += descriptor("quest_card_avatar_image", "quest_card_avatar_map", 42, 56)
    Path(sys.argv[2]).write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
