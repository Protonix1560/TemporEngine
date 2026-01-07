#!/usr/bin/env bash

dir_path="./src"

is_text_file() {
    local file="$1"
    if grep -qP '\x00' < "$file"; then
        return 1
    else
        return 0
    fi
}

total_lines=0
while IFS= read -r -d '' file; do
    if is_text_file "$file"; then
        count=$(wc -l < "$file")
        total_lines=$((total_lines + count))
    fi
done < <(find "$dir_path" -type f -print0)
echo "total lines: $total_lines"

nonempty_lines=0
while IFS= read -r -d '' file; do
    if is_text_file "$file"; then
        count=$(grep -Pv '^\s*$' "$file" | wc -l)
        nonempty_lines=$((nonempty_lines + count))
    fi
done < <(find "$dir_path" -type f -print0)
echo "non-empty lines: $nonempty_lines"

useful_lines=0
while IFS= read -r -d '' file; do
    if is_text_file "$file"; then
        count=$(grep -Pv '^\s*(\}|$)' "$file" | wc -l)
        useful_lines=$((useful_lines + count))
    fi
done < <(find "$dir_path" -type f -print0)
echo "useful lines: $useful_lines"