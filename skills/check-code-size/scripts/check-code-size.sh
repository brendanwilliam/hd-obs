#!/usr/bin/env bash
set -euo pipefail

readonly maximum_lines=800
readonly maximum_columns=90
readonly source_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)

status=0
check_file()
{
  local nonblank_line_count
  nonblank_line_count=$(awk 'NF { count++ } END { print count + 0 }' "$1")
  if ((nonblank_line_count > maximum_lines)); then
    printf 'error: %s has %s non-blank lines (maximum: %s)\n' "$1" "$nonblank_line_count" "$maximum_lines" >&2
    status=1
  fi
  awk -v maximum_columns="$maximum_columns" 'length > maximum_columns {
    printf "error: %s:%d has %d characters (maximum: %d)\\n", FILENAME, FNR, length, maximum_columns
    status = 1
  } END { exit status }' "$1" || status=1
}

while IFS= read -r -d '' path; do
  check_file "$path"
done < <(find "$source_root/src/sources" -type f \
  \( -name '*.inc' -o -name '*.cpp' -o -name '*.hpp' \) -print0)

while IFS= read -r -d '' path; do
  printf 'error: source implementation must be grouped in a subdirectory: %s\n' "$path" >&2
  status=1
done < <(find "$source_root/src/sources" -maxdepth 1 -type f \
  \( -name '*.inc' -o -name '*.cpp' -o -name '*.hpp' \) -print0)

if ((status == 0)); then
  printf 'Source modules are grouped, at or below %s non-blank lines, and at or below %s columns.\n' "$maximum_lines" "$maximum_columns"
fi

exit "$status"
