#!/usr/bin/env python3
"""Port RapidJSON's own unit tests onto rapidyyjson.

Reads RapidJSON's `test/unittest` directory and writes a build-ready copy in
which every reference to RapidJSON has been renamed to rapidyyjson. The rename
is the whole of the port for 18 of the 21 files; the handful of explicit edits
below are listed one by one, each tied to a documented divergence, so that what
the suite is *not* checking stays visible.

    ./port.py <rapidjson-checkout> <output-dir>
"""

import os
import re
import sys

# Test files that exercise machinery rapidyyjson deliberately does not provide.
# yyjson is the scanner, so RapidJSON's own number/regex internals have no
# counterpart here, and JSON Schema is out of scope.
EXCLUDED = {
    "bigintegertest.cpp": "tests internal/biginteger.h (RapidJSON's own number parser)",
    "strtodtest.cpp": "tests internal/strtod.h (RapidJSON's own number parser)",
    "regextest.cpp": "tests internal/regex.h (only used by schema.h)",
    "schematest.cpp": "tests schema.h (JSON Schema validation, not provided)",
    "simdtest.cpp": "tests the SSE2/SSE4.2/NEON scanners (yyjson does the scanning)",
}

# Edits applied on top of the rename, keyed by file. Each entry is
# (description, old, new) and every one must match exactly once.
EDITS = {
    "fwdtest.cpp": [
        (
            "drop the schema.h members (JSON Schema is not provided)",
            "    // schema.h\n"
            "    SchemaDocument* schemadocument;\n"
            "    SchemaValidator* schemavalidator;\n",
            "",
        ),
        (
            "schema.h was this file's only route to pointer.h; include it directly",
            '#include "rapidyyjson/schema.h"   // -> pointer.h',
            '#include "rapidyyjson/pointer.h"',
        ),
        (
            "drop the schema.h member initialisers",
            "    pointer(RAPIDYYJSON_NEW(Pointer)),\n"
            "\n"
            "    // schema.h\n"
            "    schemadocument(RAPIDYYJSON_NEW(SchemaDocument(*document))),\n"
            "    schemavalidator(RAPIDYYJSON_NEW(SchemaValidator(*schemadocument)))\n",
            "    pointer(RAPIDYYJSON_NEW(Pointer))\n",
        ),
        (
            "drop the schema.h member deletes",
            "    // schema.h\n"
            "    RAPIDYYJSON_DELETE(schemadocument);\n"
            "    RAPIDYYJSON_DELETE(schemavalidator);\n",
            "",
        ),
    ],
}


def rename(text):
    """The entire source-level difference between the two libraries."""
    return text.replace("RAPIDJSON_", "RAPIDYYJSON_").replace("rapidjson", "rapidyyjson")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src = os.path.join(sys.argv[1], "test", "unittest")
    dst = sys.argv[2]
    if not os.path.isdir(src):
        sys.exit("not a RapidJSON checkout: %s" % sys.argv[1])
    os.makedirs(dst, exist_ok=True)

    ported, skipped = [], []
    for name in sorted(os.listdir(src)):
        if not name.endswith((".cpp", ".h")):
            continue
        if name in EXCLUDED:
            skipped.append(name)
            continue
        text = rename(open(src + "/" + name, encoding="utf-8").read())
        for description, old, new in EDITS.get(name, []):
            if text.count(old) != 1:
                sys.exit(
                    "port.py is stale: in %s the edit %r matched %d times, expected 1.\n"
                    "RapidJSON's test suite has changed; update EDITS."
                    % (name, description, text.count(old))
                )
            text = text.replace(old, new, 1)
        open(os.path.join(dst, name), "w", encoding="utf-8").write(text)
        ported.append(name)

    print("ported %d files, skipped %d" % (len(ported), len(skipped)))
    for name in skipped:
        print("  skipped %-20s %s" % (name, EXCLUDED[name]))


if __name__ == "__main__":
    main()
