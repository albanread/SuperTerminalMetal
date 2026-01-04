#!/usr/bin/env python3
"""
Generate Font Atlas from TrueType Font
Renders a bitmap font atlas with better control over thickness and quality.
"""

import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont


def generate_font_atlas(
    font_path,
    output_path,
    cell_width=16,
    cell_height=16,
    font_size=None,
    first_char=32,
    last_char=126,
    include_box_drawing=True,
    padding=1,
):
    """
    Generate a font atlas from a TrueType font.

    Args:
        font_path: Path to the TTF font file
        output_path: Path to save the output atlas image
        cell_width: Width of each character cell in pixels
        cell_height: Height of each character cell in pixels
        font_size: Font size in points (if None, uses cell_height)
        first_char: First ASCII character to include (default: 32 = space)
        last_char: Last ASCII character to include (default: 126 = ~)
        include_box_drawing: Include box drawing characters (0x2500-0x257F)
        padding: Transparent padding pixels around each glyph (default: 1)
    """

    print(f"Loading font: {font_path}")
    print(f"Cell size: {cell_width}x{cell_height}")

    # Use font_size if provided, otherwise use cell_height
    actual_font_size = font_size if font_size is not None else cell_height
    print(f"Font size: {actual_font_size}pt")

    try:
        # Load font at the specified size
        font = ImageFont.truetype(font_path, actual_font_size)
    except Exception as e:
        print(f"Error loading font: {e}")
        return False

    # Get font metrics for baseline alignment
    ascent, descent = font.getmetrics()
    baseline = int(ascent)

    print(f"Font metrics: ascent={ascent}, descent={descent}, baseline={baseline}")

    # Build character list
    char_list = []

    # Add ASCII printable characters
    for i in range(first_char, last_char + 1):
        char_list.append(i)

    # Add box drawing characters if requested
    if include_box_drawing:
        # Box drawing characters: U+2500 to U+257F
        for i in range(0x2500, 0x2580):
            char_list.append(i)
        print(f"Including {0x2580 - 0x2500} box drawing characters")

    # Calculate atlas dimensions
    char_count = len(char_list)
    glyphs_per_row = 16  # 16 glyphs per row for nice power-of-2 layout
    rows = (char_count + glyphs_per_row - 1) // glyphs_per_row

    # Add padding to cell dimensions for the atlas
    padded_cell_width = cell_width + (padding * 2)
    padded_cell_height = cell_height + (padding * 2)

    atlas_width = glyphs_per_row * padded_cell_width
    atlas_height = rows * padded_cell_height

    # Round up to power of 2 for better GPU performance
    def next_power_of_2(n):
        power = 1
        while power < n:
            power *= 2
        return power

    atlas_width = next_power_of_2(atlas_width)
    atlas_height = next_power_of_2(atlas_height)

    print(f"Atlas size: {atlas_width}x{atlas_height}")
    print(f"Rendering {char_count} characters...")

    # Create atlas image (grayscale)
    atlas = Image.new("L", (atlas_width, atlas_height), color=0)
    draw = ImageDraw.Draw(atlas)

    # Render each character
    for i in range(char_count):
        char_code = char_list[i]
        char = chr(char_code)

        row = i // glyphs_per_row
        col = i % glyphs_per_row

        # Calculate padded cell position
        padded_cell_x = col * padded_cell_width
        padded_cell_y = row * padded_cell_height

        # Actual glyph position within the padded cell
        cell_x = padded_cell_x + padding
        cell_y = padded_cell_y + padding

        # Draw character (white on black)
        try:
            # Special handling for underscore - fill full width
            if char == "_":
                # Draw underscore manually to fill full width
                underscore_height = 2  # Thickness of underscore
                # Position underscore relative to the corrected baseline
                total_font_height = ascent + descent
                vertical_padding = (cell_height - total_font_height) // 2
                underscore_y = (
                    cell_y + vertical_padding + ascent + 1 - 2
                )  # Just below baseline, adjusted up 2 pixels
                draw.rectangle(
                    [
                        cell_x,
                        underscore_y,
                        cell_x + cell_width - 1,
                        underscore_y + underscore_height - 1,
                    ],
                    fill=255,
                )
            else:
                # Get bounding box for horizontal centering
                bbox = font.getbbox(char)
                char_width = bbox[2] - bbox[0]

                # Center horizontally, use consistent baseline for all characters
                offset_x = (cell_width - char_width) // 2

                # Calculate proper vertical position to center the font within the cell
                # The baseline should be positioned so that ascent + descent fits centered in cell_height
                total_font_height = ascent + descent
                vertical_padding = (cell_height - total_font_height) // 2
                text_y = (
                    cell_y + vertical_padding - 2
                )  # Move up 2 pixels to prevent descender clipping

                draw.text((cell_x + offset_x, text_y), char, font=font, fill=255)
        except Exception as e:
            print(f"Warning: Could not render character {char_code} ('{char}'): {e}")

    # Save atlas
    print(f"Saving atlas to: {output_path}")
    atlas.save(output_path)

    # Generate header file with metadata
    header_path = output_path.replace(".png", "_metadata.h")
    with open(header_path, "w") as f:
        f.write(f"""// Auto-generated font atlas metadata
// Generated from: {os.path.basename(font_path)}

#ifndef FONT_ATLAS_METADATA_H
#define FONT_ATLAS_METADATA_H

#define FONT_ATLAS_WIDTH {atlas_width}
#define FONT_ATLAS_HEIGHT {atlas_height}
#define FONT_GLYPH_WIDTH {cell_width}
#define FONT_GLYPH_HEIGHT {cell_height}
#define FONT_ATLAS_PADDING {padding}
#define FONT_PADDED_CELL_WIDTH {padded_cell_width}
#define FONT_PADDED_CELL_HEIGHT {padded_cell_height}
#define FONT_FIRST_CHAR {first_char}
#define FONT_LAST_CHAR {last_char}
#define FONT_GLYPHS_PER_ROW {glyphs_per_row}
#define FONT_CHAR_COUNT {char_count}
#define FONT_INCLUDES_BOX_DRAWING {1 if include_box_drawing else 0}

// Character map: maps Unicode codepoint to atlas index
// First {last_char - first_char + 1} entries are ASCII {first_char}-{last_char}
// Remaining entries are box drawing U+2500-U+257F
// Note: Atlas uses padded cells ({padded_cell_width}x{padded_cell_height}) with {padding}px padding

#endif // FONT_ATLAS_METADATA_H
""")

    print(f"Metadata saved to: {header_path}")
    print("Done!")

    return True


def main():
    # Get script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Default paths
    font_path = os.path.join(script_dir, "../../Assets/font/petme/PetMe128.ttf")
    output_path = os.path.join(script_dir, "font_atlas.png")

    # Allow command-line override
    if len(sys.argv) > 1:
        font_path = sys.argv[1]
    if len(sys.argv) > 2:
        output_path = sys.argv[2]

    cell_width = 16  # Default cell width
    cell_height = 16  # Default cell height
    font_size = None  # Default font size (uses cell_height)
    padding = 1  # Default padding

    if len(sys.argv) > 3:
        cell_width = int(sys.argv[3])
    if len(sys.argv) > 4:
        cell_height = int(sys.argv[4])
    else:
        cell_height = cell_width  # Square cells by default
    if len(sys.argv) > 5:
        font_size = int(sys.argv[5])
    if len(sys.argv) > 6:
        padding = int(sys.argv[6])

    if not os.path.exists(font_path):
        print(f"Error: Font file not found: {font_path}")
        print(
            f"Usage: {sys.argv[0]} [font_path] [output_path] [cell_width] [cell_height] [font_size] [padding]"
        )
        return 1

    success = generate_font_atlas(
        font_path,
        output_path,
        cell_width,
        cell_height,
        font_size,
        include_box_drawing=True,
        padding=padding,
    )
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
