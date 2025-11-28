#!/usr/bin/env python3

from PIL import Image
from pathlib import Path

ALBUM_COVER_WIDTH = 152
ALBUM_COVER_HEIGHT = 150

INPUT_PATH = Path("raw_covers")
OUTPUT_PATH = Path("bmp_covers")

def conv_to_bmp(file_path: Path):
    try:
        img = Image.open(file_path)
        img = img.resize((ALBUM_COVER_WIDTH, ALBUM_COVER_HEIGHT), Image.LANCZOS)
        img.save(OUTPUT_PATH / Path(file_path.stem + ".bmp"), format="BMP")
        print(f'Converted {file_path} into BMP...')
        print(f"rgb: {img.getpixel((0, 0))}")
    except:
        print(f'Failed to convert {file_path} into BMP...')

def main():
    files = [entry for entry in INPUT_PATH.iterdir() if entry.is_file()]
    for file in files:
        conv_to_bmp(file)

if __name__ == "__main__":
    main()
