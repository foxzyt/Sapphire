# Third-Party Libraries

This directory should contain the following third-party libraries:

## Required Libraries

1. **httplib.h** - HTTP/HTTPS client library
   - Download from: https://github.com/yhirose/cpp-httplib
   - Place the single header file `httplib.h` in this directory
   - Requires OpenSSL for HTTPS support (already configured in CMakeLists.txt)

2. **termcolor.hpp** - Terminal color formatting
   - Download from: https://github.com/ikalnitsky/termcolor
   - Place the header file `termcolor.hpp` in this directory
   - Alternatively, use the existing `termcolor.cpp` from `../../src/utils/`

3. **miniz.c / miniz.h** - ZIP extraction library
   - Download from: https://github.com/richgel999/miniz
   - Place both `miniz.c` and `miniz.h` in this directory
   - This is a single-file zlib replacement

4. **json.hpp** - nlohmann/json library
   - Download from: https://github.com/nlohmann/json
   - Place the single header file `json.hpp` in this directory
   - This is a header-only library

## Implementation Notes

Once these libraries are added, uncomment the relevant includes in:
- `include/core/downloader.hpp` (for httplib and json)
- `include/core/resolver.hpp` (for termcolor)
- `src/main.cpp` (for termcolor)

And implement the placeholder functions in:
- `include/core/downloader.hpp` (parse_registry_json, query_registry, download_file, extract_zip)
