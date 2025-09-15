#!/usr/bin/python3
import time
import os
import sys
import subprocess
import random
from datetime import datetime

# pragma-new: emit a template for a new pragma web page source and write it to disk.
# 
# By default, this helper creates a template for a post on the current date, but it
# accepts an argument if the user would like to backdate (or future-date) a post. 
#
# usage: ./pragma-new [Month DD YYY HH:MM]
#
# example: ./pragma-new January 1 2001 15:30
#
# Because pragma-web works backwards from the current system time when publishing the
# site, it will not publish future-dated posts, so they are inherently drafts until
# their publication time arrives. Or until someone changes the system time.

def load_dictionary_words():
    """Load words from system dictionary, filtering for reasonable post filename words."""
    dictionary_paths = [
        '/usr/share/dict/words',     # Common on Linux/macOS
        '/usr/dict/words',           # Alternative location
        '/etc/dictionaries-common/words'  # Debian/Ubuntu
    ]

    words = []
    for path in dictionary_paths:
        if os.path.exists(path):
            try:
                with open(path, 'r') as f:
                    # Filter for reasonable words: 3-8 characters, lowercase, no apostrophes
                    words = [line.strip().lower() for line in f
                           if 3 <= len(line.strip()) <= 8
                           and line.strip().isalpha()
                           and line.strip().islower()]
                break
            except:
                continue

    # Fallback word list if no system dictionary found
    if not words:
        words = [
            'apple', 'brave', 'cloud', 'dream', 'earth', 'flame', 'green', 'happy',
            'idea', 'jump', 'kind', 'light', 'magic', 'north', 'ocean', 'peace',
            'quick', 'river', 'smile', 'tree', 'under', 'voice', 'water', 'young',
            'dance', 'bright', 'swift', 'gentle', 'strong', 'quiet', 'warm', 'fresh'
        ]

    return words

def generate_filename():
    """Generate a filename using three random dictionary words."""
    words = load_dictionary_words()

    # Try to generate a unique filename
    max_attempts = 3 #let's be realistic...
    for _ in range(max_attempts):
        # Select three random words
        selected_words = random.sample(words, 3)
        filename = "_".join(selected_words) + ".txt"

        if not os.path.exists(filename):
            return filename

    # Fallback to timestamp if we can't find a unique name
    return str(int(time.time())) + ".txt"

def get_user_filename(suggested_filename):
    """Prompt user to accept or modify the suggested filename."""
    print(f"Suggested filename: {suggested_filename}")
    user_input = input("Press Enter to accept, or type a new filename: ").strip()

    if not user_input:
        return suggested_filename

    # Ensure .txt extension
    if not user_input.endswith('.txt'):
        user_input += '.txt'

    # Check if the custom filename already exists
    if os.path.exists(user_input):
        print(f"Warning: {user_input} already exists!")
        overwrite = input("Overwrite? (y/N): ").strip().lower()
        if overwrite != 'y':
            print("Aborting to avoid overwriting existing file.")
            sys.exit(1)

    return user_input

now = time.time();

if ( len(sys.argv[1:] ) > 0 ):
	args = sys.argv[0:];
	argument = ' '.join(str(a) for a in args[1:]);
	print(argument.strip());
	# can we parse this into a reasonable date?
	dt = datetime.strptime(argument.strip(), '%B %d %Y %H:%M');
	try:
		epoch = dt.timestamp();
	except ValueError:
		print("Error: can't convert the argument", argument, "to a timestamp. Aborting!");
		exit();
	print("Got an argument => converted to ", epoch);	
	now = epoch;

# Generate and get user approval for filename
suggested_filename = generate_filename()
fn = get_user_filename(suggested_filename)

output = """title:None
tags:
summary:
static_icon:
parse:
date:""" + str(now) + """
###
Your content here (Markdown and/or HTML can mix freely, see README)"""

print(output, file=open(fn, 'w'))

# Create image directory based on the chosen filename (without .txt extension)
base_name = fn[:-4] if fn.endswith('.txt') else fn
image_dir = "../img/" + base_name
print("Created " + fn + ".")

# Create a directory for images associated with this post, use the 'open' command to display that
# directory in the Finder, and open the new template file in an editor. Portability should improve.
#
# By default, the pramga-new helper opens vim to edit the resulting file, but the invoked executable
# can be changed to, e.g., TextEdit, VS Code, or even emacs.
subprocess.run(["mkdir", image_dir]);
subprocess.run(["open", image_dir]);
subprocess.run(["vim", fn]);
