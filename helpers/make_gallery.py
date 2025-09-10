#!/usr/bin/env python3
"""
Image gallery generator for pragma-web.

Generates thumbnails and GLightbox-compatible HTML markup for image galleries.
Processes directories containing images and creates corresponding thumbnail files.

Author: Will Shaw <wsshaw@gmail.com>
Project: pragma-web
"""
import os
import sys
from PIL import Image

THUMB_SIZE = (256, 256)  # max width/height

def generate_thumbnails(directory):
    """Generate thumbnails for each image in the directory."""
    for filename in os.listdir(directory):
        # Skip non-image files
        if not filename.lower().endswith(('.jpg', '.jpeg', '.png')):
            continue

        # Skip files that are already thumbnails
        if filename.startswith('thumb'):
            continue

        full_path = os.path.join(directory, filename)

        # Construct thumbnail filename
        base_name, ext = os.path.splitext(filename)
        thumb_name = f"thumb{base_name}{ext}"
        thumb_path = os.path.join(directory, thumb_name)

        # Skip if thumbnail already exists
        if os.path.exists(thumb_path):
            continue

        # Generate thumbnail
        try:
            with Image.open(full_path) as img:
                img.thumbnail(THUMB_SIZE)
                img.save(thumb_path)
                print(f"Generated thumbnail: {thumb_path}")
        except Exception as e:
            print(f"Failed to generate thumbnail for {filename}: {e}")

def generate_gallery_html(directory, gallery_name='mygallery'):
    """Generate GLightbox HTML markup for images in the directory."""
    directory = directory.rstrip('/')
    files = [f for f in os.listdir(directory) if f.lower().endswith(('.jpg', '.jpeg', '.png'))]
    files.sort()

    html_lines = []
    for file in files:
        if file.startswith('thumb'):
            continue

        base_name, ext = os.path.splitext(file)
        thumb_name = f"thumb{base_name}{ext}"

        # Build HTML
        line = f'''<a href="{os.path.join(directory, file)}" class="glightbox" 
 	data-glightbox="descPosition: right;"
	data-gallery="{gallery_name}" 
	data-title="" 
	data-description="">
		<img src="{os.path.join(directory, thumb_name)}" alt="{base_name}">
</a>'''
        html_lines.append(line)

    return '\n'.join(html_lines)

def main():
    if len(sys.argv) != 2:
        print("Usage: python gallery.py <directory>")
        sys.exit(1)

    directory = sys.argv[1]

    if not os.path.isdir(directory):
        print(f"Error: '{directory}' is not a valid directory.")
        sys.exit(1)

    generate_thumbnails(directory)
    html_output = generate_gallery_html(directory)
    print(html_output)

if __name__ == '__main__':
    main()
