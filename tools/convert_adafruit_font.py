#!/usr/bin/env python3
"""Convierte una fuente OpenType/TrueType a un encabezado Adafruit GFX.

Uso:
  python tools/convert_adafruit_font.py fuente.otf salida.h NombreDeFuente 8

Sólo genera caracteres ASCII imprimibles (0x20–0x7E), que cubren la UI
actual y mantiene bajo el uso de flash de la ESP32.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


FIRST_CHAR = 0x20
LAST_CHAR = 0x7E
PIXEL_THRESHOLD = 127


def format_bytes(values: list[int]) -> str:
    """Devuelve los bytes en líneas C++ cortas y fáciles de revisar."""
    if not values:
        return "  0x00"
    lines = []
    for index in range(0, len(values), 12):
        lines.append("  " + ", ".join(f"0x{value:02X}" for value in values[index:index + 12]))
    return ",\n".join(lines)


def generate_header(font_path: Path, output_path: Path, font_name: str, pixel_size: int) -> None:
    font = ImageFont.truetype(str(font_path), pixel_size)
    bitmaps: list[int] = []
    glyphs: list[tuple[int, int, int, int, int, int]] = []

    for codepoint in range(FIRST_CHAR, LAST_CHAR + 1):
        character = chr(codepoint)
        x0, y0, x1, y1 = font.getbbox(character, anchor="ls")
        width = max(0, x1 - x0)
        height = max(0, y1 - y0)
        bitmap_offset = len(bitmaps)
        x_advance = round(font.getlength(character))

        if width and height:
            image = Image.new("L", (width, height), 0)
            drawer = ImageDraw.Draw(image)
            drawer.text((-x0, -y0), character, font=font, fill=255, anchor="ls")

            packed_byte = 0
            bit_count = 0
            for pixel in image.get_flattened_data():
                packed_byte = (packed_byte << 1) | int(pixel > PIXEL_THRESHOLD)
                bit_count += 1
                if bit_count == 8:
                    bitmaps.append(packed_byte)
                    packed_byte = 0
                    bit_count = 0
            if bit_count:
                bitmaps.append(packed_byte << (8 - bit_count))

        glyphs.append((bitmap_offset, width, height, x_advance, x0, y0))

    ascent, descent = font.getmetrics()
    y_advance = ascent + descent
    header_guard = f"{font_name.upper()}_H"
    glyph_rows = []
    for codepoint, glyph in zip(range(FIRST_CHAR, LAST_CHAR + 1), glyphs):
        offset, width, height, advance, x_offset, y_offset = glyph
        glyph_rows.append(
            f"  {{{offset}, {width}, {height}, {advance}, {x_offset}, {y_offset}}}, "
            f"// 0x{codepoint:02X} '{chr(codepoint)}'"
        )

    output = f"""// Archivo generado desde {font_path.name}; no editar a mano.
// Fuente de la interfaz Pip-Boy. Consulte fonts/README.md.
#pragma once

#include <Adafruit_GFX.h>

const uint8_t {font_name}Bitmaps[] PROGMEM = {{
{format_bytes(bitmaps)}
}};

const GFXglyph {font_name}Glyphs[] PROGMEM = {{
{chr(10).join(glyph_rows)}
}};

const GFXfont {font_name} PROGMEM = {{
  (uint8_t *){font_name}Bitmaps,
  (GFXglyph *){font_name}Glyphs,
  0x{FIRST_CHAR:02X}, 0x{LAST_CHAR:02X}, {y_advance}
}};

// Aproximadamente {len(bitmaps) + len(glyphs) * 7} bytes en flash.
"""
    output_path.write_text(output, encoding="ascii", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("font_name")
    parser.add_argument("pixel_size", type=int)
    args = parser.parse_args()

    if args.pixel_size < 6:
        parser.error("El tamaño debe ser de al menos 6 píxeles.")
    generate_header(args.font, args.output, args.font_name, args.pixel_size)


if __name__ == "__main__":
    main()
