# RapidJSON's own unit tests, run against rapidyyjson

The strongest conformance evidence available for a library that claims to
reimplement RapidJSON is RapidJSON's own test suite. This directory fetches
RapidJSON at the tag whose API rapidyyjson reproduces, renames every reference
to it, and runs the result against these headers.

```bash
cmake -S . -B build -DRAPIDYYJSON_BUILD_RAPIDJSON_SUITE=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

RapidJSON and googletest are fetched at configure time, so this needs network
access — which is why the option is off by default.

## What actually runs

**204 of RapidJSON's 223 tests pass unmodified.** The remaining 19 are excluded
by an explicit `--gtest_filter` in `CMakeLists.txt`, each mapped to a numbered
entry in the root README's divergence list. The list is an exclusion rather than
a deletion on purpose: everything else must pass, so a regression anywhere in the
suite fails the build.

To see the divergences rather than skip them:

```bash
cmake --build build --target rapidjson-suite-full
```

## The port

`port.py` does the rename that is the whole source-level difference between the
two libraries — `rapidjson` → `rapidyyjson`, `RAPIDJSON_` → `RAPIDYYJSON_` — and
nothing else for 20 of the 21 files it emits. The exception is `fwdtest.cpp`,
where four edits remove the `schema.h` members; each edit is spelled out in
`EDITS` and must match exactly once, so the script fails loudly rather than
silently skipping if RapidJSON's tests change.

Five test files are not ported at all, because they test machinery that has no
counterpart here — yyjson is the scanner, so RapidJSON's own number parsing and
regex internals are simply absent:

| File | Why |
| ---- | --- |
| `bigintegertest.cpp` | `internal/biginteger.h`, part of RapidJSON's number parser |
| `strtodtest.cpp` | `internal/strtod.h`, likewise |
| `regextest.cpp` | `internal/regex.h`, only used by `schema.h` |
| `schematest.cpp` | `schema.h`, JSON Schema validation, not provided |
| `simdtest.cpp` | the SSE2/SSE4.2/NEON scanners |

## What it found

Running the suite for the first time turned up five real defects and two missing
pieces of API, all fixed and now pinned by `TestSuiteRegressions()` in
`../conformance.cc`:

* `GenericStringStream` with a non-UTF8 encoding handed yyjson raw UTF-16/UTF-32
  code units instead of transcoding them, so **no wide string stream parsed at all**.
* An empty transcoded string was passed to handlers as a `const char*` NUL
  reinterpreted as a wide character, reading past the object. Transcoded strings
  are now NUL-terminated, as RapidJSON's are.
* `kParseStopWhenDoneFlag` repositioned the cursor relative to the head of the
  stream rather than to where the parse began, so a second parse re-read the
  first value.
* In-situ string streams had no seek support at all, so `kParseStopWhenDoneFlag`
  left them drained.
* `RAPIDYYJSON_64BIT` was never defined, which made `RAPIDYYJSON_ALIGN` and any
  client code guarded on it take the 32-bit branch.
* `internal::Double` (`internal/ieee754.h`) was missing.
* `SkipWhitespace` and `internal::StreamLocalCopy` were missing, and 30 of the
  `*ValueByPointer` free-function overloads (`GetValueByPointerWithDefault` had
  2 of RapidJSON's 16, `SetValueByPointer` 6 of 20).
