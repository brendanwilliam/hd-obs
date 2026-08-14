---
name: check-code-size
description: Check source-file size, ownership, and grouping before or after a refactor. Use when adding substantial C++, reviewing source layout, or splitting a large implementation.
---

# Check code size

1. Run `skills/check-code-size/scripts/check-code-size.sh`.
2. Keep every source implementation module at or below 800 non-blank lines. Treat the limit as a
   ceiling: split files by stable responsibility whenever that improves ownership or readability,
   never by arbitrary line range.
3. Keep code lines at or below 90 characters. Use logical line breaks, indentation, and blank lines
   to make control flow and data construction readable; do not compress code to avoid refactoring.
4. Group each source implementation under a feature or shared-responsibility directory in
   `src/sources/`; implementation files do not belong directly in `src/sources/`.
5. Verify shared setting keys, migrations, and rendering helpers have one owner. Source-specific
   rendering and property code belong with that source type.
6. Format edited C++ with clang-format 19 and run the macOS CI-equivalent build.
