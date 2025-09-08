#!/usr/bin/python3
import time
import os
import sys
import subprocess
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

output = """title:None
tags:
date:""" + str(now) + """
###
Your content here (Markdown or HTML)"""
	
fn = str(now) + ".txt";

# This scenario is pretty unlikely--that there's already a file with exactly the current epoch
# as its name. But just in case...
if (os.path.exists(fn)):
        print("Error:",fn,"already exists.  Try again in 1ms.");
        exit();

print(output, file=open(str(now) + ".txt", 'w'));
image_dir = "../img/" + str(now);
print("Created " + str(now) + ".txt.");

# Create a directory for images associated with this post, use the 'open' command to display that
# directory in the Finder, and open the new template file in an editor. Portability should improve.
#
# By default, the pramga-new helper opens vim to edit the resulting file, but the invoked executable
# can be changed to, e.g., TextEdit, VS Code, or even emacs.
subprocess.run(["mkdir", image_dir]);
subprocess.run(["open", image_dir]);
subprocess.run(["vim", fn]);
