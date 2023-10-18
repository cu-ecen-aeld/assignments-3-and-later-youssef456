#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <directory_or_file_path> <search_string>"
    exit 1
fi

path="$1"
search_string="$2"

# Remove any trailing slash if present
path=${path%/}

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$path")"

# Use grep to search for the string in files and count matching lines
match_count=$(grep -r "$search_string" "$path" | wc -l)

# Count the number of files in the directory and subdirectories
file_count=$(find "$path" -type f | wc -l)

echo "The number of files are $file_count and the number of matching lines are $match_count"

