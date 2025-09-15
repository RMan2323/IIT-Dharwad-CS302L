#!/bin/bash

# --- Script to extract the first and last 10 lines from files in a directory ---

# 1. Check if a directory was provided as an argument.
if [ -z "$1" ]; then
  echo "❌ Error: No directory supplied."
  echo "Usage: ./extract_lines.sh <directory_path>"
  exit 1
fi

TARGET_DIR=$1

# 2. Check if the provided argument is actually a directory.
if [ ! -d "$TARGET_DIR" ]; then
  echo "❌ Error: '$TARGET_DIR' is not a valid directory."
  exit 1
fi

# 3. Define the output file name based on the directory name.
# The 'basename' command removes any leading directory paths.
DIR_NAME=$(basename "$TARGET_DIR")
OUTPUT_FILE="${DIR_NAME}-output.txt"

# 4. Announce the start and clear any previous output file.
echo "Processing files in '$TARGET_DIR'..."
echo "Output will be saved to '$OUTPUT_FILE'."
# This creates/clears the output file.
> "$OUTPUT_FILE"

# 5. Loop through each file in the target directory.
for file in "$TARGET_DIR"/*; do
  # Check if the item is a regular file (and not a directory).
  if [ -f "$file" ]; then
    echo "  -> Processing $(basename "$file")"
    
    # 6. Append the file's name and content snippets to the output file.
    {
      echo "=================================================="
      echo "✅ FILENAME: $(basename "$file")"
      echo "=================================================="
      echo ""
      echo "--- First 10 lines ---"
      head -n 10 "$file"
      echo ""
      echo "--- Last 10 lines ---"
      tail -n 10 "$file"
      echo ""
      echo ""
    } >> "$OUTPUT_FILE"
  fi
done

echo "✨ All done!"
