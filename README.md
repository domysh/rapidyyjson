# rapidyyjson

[![CI](https://github.com/domysh/rapidyyjson/actions/workflows/ci.yml/badge.svg)](https://github.com/domysh/rapidyyjson/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A drop-in reimplementation of the **RapidJSON 1.1.0** public API on top of
[yyjson](https://github.com/ibireme/yyjson).

Header-only, C++17, ~11k lines. Existing RapidJSON code compiles and behaves
unchanged after a mechanical rename — only three things differ at the source
level:

| RapidJSON             | rapidyyjson             |
| --------------------- | ----------------------- |
| `namespace rapidjson` | `namespace rapidyyjson` |
| `RAPIDJSON_*` macros  | `RAPIDYYJSON_*` macros  |
| `<rapidjson/x.h>`     | `<rapidyyjson/x.h>`     |

Class names, template parameters, overload sets, return types, default
arguments, assertion points, iterator categories and output formatting are all
the same.

## Why

RapidJSON's last release is 1.1.0 (2016), so projects that depend on it usually
end up vendoring a copy and carrying it forever. rapidyyjson lets you keep every
line of that RapidJSON code while the actual scanning is done by yyjson — which
is actively maintained, packaged by the major distributions, and fast.

It is useful if you want to:

* **drop a vendored RapidJSON** in favour of a packaged system dependency,
  without rewriting the call sites;
* **migrate to yyjson gradually**, keeping the DOM API your code is written
  against while the parser underneath is already the new one;
* **build against a maintained parser** for the security and correctness fixes
  that a 2016 release no longer gets.

It is *not* a good fit if you depend on `schema.h` (JSON Schema validation) or
on RapidJSON's SIMD opt-in macros — see [Known divergences](#known-divergences).

## Requirements

* A C++17 compiler (GCC, Clang and AppleClang are covered by CI)
* [yyjson](https://github.com/ibireme/yyjson) ≥ 0.5.0, headers and library
  (the conformance suite is verified against 0.5.0, 0.6.0, 0.8.0 and 0.12.0;
  older versions are rejected with a `#error`)
* CMake ≥ 3.16, if you use the CMake integration

Installing yyjson:

```bash
sudo apt install libyyjson-dev      # Debian / Ubuntu
brew install yyjson                 # macOS
sudo dnf install yyjson-devel       # Fedora / RHEL
sudo pacman -S yyjson               # Arch
vcpkg install yyjson                # vcpkg
```

Or build it from source:

```bash
git clone --depth 1 --branch 0.12.0 https://github.com/ibireme/yyjson.git
cmake -S yyjson -B yyjson/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build yyjson/build --parallel && sudo cmake --install yyjson/build
```

## Using it in your project

The library is header-only, so there is nothing to compile — you only need the
`include/` directory on your include path and `libyyjson` on your link line.

### As a CMake subproject (git submodule)

```bash
git submodule add https://github.com/domysh/rapidyyjson.git 3rdparty/rapidyyjson
```

```cmake
add_subdirectory(3rdparty/rapidyyjson)
target_link_libraries(my_target PRIVATE rapidyyjson::rapidyyjson)
```

The target carries the include directory, the C++17 requirement and the
`yyjson::yyjson` link dependency, so nothing else is needed. Tests and the
install target switch themselves off when the project is consumed this way.

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(rapidyyjson
    GIT_REPOSITORY https://github.com/domysh/rapidyyjson.git
    GIT_TAG        main)
FetchContent_MakeAvailable(rapidyyjson)

target_link_libraries(my_target PRIVATE rapidyyjson::rapidyyjson)
```

### As an installed package

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --install build --prefix /usr/local
```

```cmake
find_package(rapidyyjson REQUIRED)
target_link_libraries(my_target PRIVATE rapidyyjson::rapidyyjson)
```

The installed package re-runs the same yyjson lookup that the build used
(yyjson's own CMake config → `pkg-config` → a plain header/library search), so
consumers do not have to find yyjson themselves.

### Without CMake

Copy `include/rapidyyjson` into your source tree, or point at it directly:

```bash
c++ -std=c++17 -Ipath/to/rapidyyjson/include my_app.cc -lyyjson -o my_app
```

## Migrating an existing RapidJSON codebase

The rename is mechanical and reversible:

```bash
# GNU sed (Linux); on macOS use `sed -i ''` instead of `sed -i`
grep -rlZ -e rapidjson -e RAPIDJSON_ src/ include/ \
  | xargs -0 sed -i -e 's/rapidjson/rapidyyjson/g' -e 's/RAPIDJSON_/RAPIDYYJSON_/g'
```

Then swap the include path and add `-lyyjson`.

If you would rather not touch the call sites at all, a shim directory works too:
put headers named after the RapidJSON ones somewhere earlier on the include
path, each forwarding to rapidyyjson and re-aliasing the namespace.

```cpp
// compat/rapidjson/document.h
#pragma once
#include <rapidyyjson/document.h>
namespace rapidjson = rapidyyjson;
```

Existing `#include <rapidjson/document.h>` lines and `rapidjson::` qualifications
then keep working unchanged. Code that spells out `RAPIDJSON_*` macros still has
to be renamed — a macro cannot be aliased.

Read [Known divergences](#known-divergences) before you migrate — most codebases
are unaffected, but the parse-error codes and `kParseInsituFlag` are worth
checking.

## A quick tour

```cpp
#include <rapidyyjson/document.h>
#include <rapidyyjson/prettywriter.h>
#include <rapidyyjson/stringbuffer.h>

rapidyyjson::Document d;
d.Parse(R"({"name": "drone-01", "battery": 0.87})");
if (d.HasParseError()) { /* d.GetParseError(), d.GetErrorOffset() */ }

const char* name = d["name"].GetString();

auto& alloc = d.GetAllocator();
d.AddMember("armed", true, alloc);

rapidyyjson::StringBuffer sb;
rapidyyjson::PrettyWriter<rapidyyjson::StringBuffer> writer(sb);
d.Accept(writer);
std::puts(sb.GetString());
```

[`example/basic.cc`](example/basic.cc) is a longer, buildable version of the
same tour (parsing, DOM inspection, mutation, JSON Pointer, serialisation).

## Headers

| Header | Contents |
| ------ | -------- |
| `rapidyyjson.h` | `Type`, `SizeType`, configuration and diagnostic macros |
| `allocators.h` | `CrtAllocator`, `MemoryPoolAllocator` |
| `encodings.h` | `UTF8`, `UTF16(LE\|BE)`, `UTF32(LE\|BE)`, `ASCII`, `AutoUTF`, `Transcoder` |
| `stream.h` | Stream concept, `StringStream`, `InsituStringStream`, `GenericStreamWrapper` |
| `error/error.h`, `error/en.h` | `ParseErrorCode`, `ParseResult`, `GetParseError_En()` |
| `reader.h` | `ParseFlag`, `BaseReaderHandler`, `GenericReader` / `Reader` |
| `document.h` | `GenericValue` / `Value`, `GenericDocument` / `Document`, `GenericArray`, `GenericObject`, `GenericMember(Iterator)`, `GenericStringRef` / `StringRef()` |
| `writer.h` | `WriteFlag`, `Writer` |
| `prettywriter.h` | `PrettyFormatOptions`, `PrettyWriter` |
| `stringbuffer.h`, `memorybuffer.h`, `memorystream.h` | in-memory streams |
| `filereadstream.h`, `filewritestream.h` | `FILE*` streams |
| `istreamwrapper.h`, `ostreamwrapper.h` | `std::basic_i/ostream` wrappers |
| `encodedstream.h` | `EncodedInput/OutputStream`, `AutoUTFInput/OutputStream` |
| `cursorstreamwrapper.h` | `CursorStreamWrapper` (line/column tracking) |
| `pointer.h` | `GenericPointer` / `Pointer` and the `*ValueByPointer()` helpers |
| `fwd.h` | forward declarations |
| `internal/` | `meta.h`, `stack.h`, `strfunc.h`, `swap.h`, `itoa.h`, `dtoa.h` |

## Where yyjson sits

yyjson is the **scanner**. `GenericReader::Parse()` collects the input stream,
hands it to `yyjson_read_opts()`, and replays the resulting tree as the SAX event
sequence RapidJSON handlers expect (iteratively, so nesting depth is bounded by
the heap rather than the call stack).

Everything above the scanner is a native implementation that reproduces
RapidJSON's data model: the tagged-union DOM with its flag layout, the
allocator ownership rules, the writers, and JSON Pointer. No `yyjson_val` ever
escapes into the DOM, which is what makes the move semantics, in-place mutation
and allocator lifetimes match RapidJSON exactly.

## Semantics worth restating

These are RapidJSON behaviours that are easy to trip over and are reproduced
here deliberately:

* **`Value` is move-only.** The copy constructor is deleted; `operator=` and
  `PushBack`/`AddMember` *move* their argument, leaving it null. Use
  `Value(rhs, allocator)` or `CopyFrom()` for a deep copy.
* **`IsDouble()` is a storage test, not a conversion test.** `500` parses as an
  integer, so `IsDouble()` is `false` while `GetDouble()` still returns `500.0`.
  Only `500.0` sets the double flag.
* **`operator[]` asserts.** Reading a missing member or an out-of-range index
  aborts in a debug build; guard with `HasMember()`/`FindMember()`/`Size()`.
  `HasMember()`, `MemberCount()`, `Size()` etc. themselves assert on the wrong
  value type.
* **`RemoveMember()` reorders** (it moves the last member into the hole);
  `EraseMember()` preserves order.
* **`MemoryPoolAllocator` never frees.** Memory is reclaimed when the owning
  `Document` (or the allocator) is destroyed, which is why `~GenericValue()` is
  a no-op for the default allocator.
* **Doubles are written with the shortest round-trippable representation**, in
  RapidJSON's layout (`1.0`, `0.000001`, `1e30`, `1.234e33`).

## Known divergences

1. **Parse error codes and offsets are approximate.** `HasParseError()` and
   `ParseResult`'s success/failure are exact, but the specific `ParseErrorCode`
   is derived from yyjson's error code and message, so a malformed document may
   report a neighbouring code (often `kParseErrorUnspecificSyntaxError`) where
   RapidJSON would name the exact production, and `GetErrorOffset()` is yyjson's
   error position rather than RapidJSON's.
2. **`kParseInsituFlag` is accepted but not destructive.** `ParseInsitu()`
   yields the same DOM, but the source buffer is left untouched and strings are
   copied into the allocator instead of pointing into it.
3. **`kParseStopWhenDoneFlag`** leaves the cursor just past the root value for
   `GenericStringStream` and `GenericInsituStringStream`; for other stream types
   the stream is drained to its end even though parsing stopped earlier.
4. **`kParseIterativeFlag`, `kParseFullPrecisionFlag` and
   `kParseValidateEncodingFlag` are no-ops** because the scanner is already
   iterative, exact, and UTF-8-validating.
5. **`GenericReader::IterativeParseNext()`** materialises the whole event stream
   on the first call and then hands out one event per call.
6. **Not provided:** `schema.h` (JSON Schema validation), `RAPIDJSON_SSE2` /
   `RAPIDJSON_NEON` opt-in macros, and pointer-compression
   (`RAPIDJSON_48BITPOINTER_OPTIMIZATION`). There is also no short-string inline
   optimisation, so `sizeof(Value)` is 24 rather than 16 — an internal detail
   with no API consequence.
7. **A handler can only terminate a parse of well-formed input.** The whole
   document is scanned before the first event is emitted, so when a handler
   returns `false` on input that is *also* malformed, the scanner's syntax error
   is reported instead of `kParseErrorTermination`. For valid input,
   `kParseErrorTermination` is reported as RapidJSON does, but its offset is the
   number of bytes scanned rather than the position of the terminating event.
8. **`kParseNanAndInfFlag` is more permissive.** yyjson also accepts spellings
   like `nan` and `NAN`, which RapidJSON rejects with
   `kParseErrorValueInvalid`.
9. **`StringRef(0, 0)` is tolerated**, following current RapidJSON, and yields
   an empty string; RapidJSON 1.1.0 asserts on any null pointer here.

## Tests

[`test/conformance.cc`](test/conformance.cc) is a standalone suite (~310
assertions) that pins the semantics above: type flags, move semantics,
object/array mutation and bulk growth, string handling including embedded nulls,
equality and deep copy across allocators, `Is<T>`/`Get<T>`/`Set<T>`, writer
escaping and shortest round-trip number formatting, pretty printing, parse errors
and every parse flag, all stream types, UTF-8/16/ASCII transcoding, the SAX
reader (including the iterative interface), JSON Pointer with RFC 6901's own
examples, document move semantics, and the allocators. A final `TestApiSurface()`
instantiates the templates the behavioural tests do not otherwise reach.

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

It has no test-framework dependency, so it also builds by hand:

```bash
c++ -std=c++17 -Iinclude test/conformance.cc -lyyjson -o conformance && ./conformance
```

### RapidJSON's own suite

Since RapidJSON 1.1.0 is the specification, its unit tests are the real
conformance check, and they run here too:
[`test/rapidjson-suite`](test/rapidjson-suite) fetches RapidJSON at `v1.1.0`,
puts its `test/unittest` sources through the same mechanical rename described
above, and builds them against these headers.

```bash
cmake -S . -B build -DRAPIDYYJSON_BUILD_RAPIDJSON_SUITE=ON
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

**204 of RapidJSON's 223 tests pass unmodified.** The other 19 are excluded by an
explicit filter, each one tied to a numbered entry in
[Known divergences](#known-divergences) — mostly the error-code and error-offset
tests. Five of RapidJSON's 26 test files are not ported at all, because they test
its own number-parsing, regex and SIMD internals, which yyjson replaces outright.
[The suite's README](test/rapidjson-suite/README.md) has the full accounting, and
lists the seven defects and missing API pieces the suite found on its first run.

The option is off by default because it needs network access to fetch RapidJSON
and googletest.

## CMake options

| Option | Default | Effect |
| ------ | ------- | ------ |
| `RAPIDYYJSON_BUILD_TESTS` | `ON` when top-level, else `OFF` | Build and register the conformance suite |
| `RAPIDYYJSON_INSTALL` | `ON` when top-level, else `OFF` | Generate the install and export targets |
| `RAPIDYYJSON_BUILD_RAPIDJSON_SUITE` | `OFF` | Also build RapidJSON's own unit tests against this library (fetches RapidJSON and googletest) |

## Versioning

The project version (`0.1.0`) tracks this library. The `RAPIDYYJSON_*_VERSION`
macros report `1.1.0` — the RapidJSON API level being reproduced — so that code
guarded on `RAPIDJSON_VERSION_STRING` keeps working after the rename.

## Contributing

Issues and pull requests are welcome. Anything that changes observable behaviour
should come with an assertion in `test/conformance.cc`, and the rule for
resolving a question is simple: **RapidJSON 1.1.0 is the specification.** If the
two disagree and it is not on the divergence list above, that is a bug here —
and RapidJSON's own suite is wired up to say so.

## License

MIT — see [LICENSE](LICENSE).

rapidyyjson is an independent implementation; it contains no RapidJSON source.
[RapidJSON](https://github.com/Tencent/rapidjson) (© Tencent, MIT) is the API it
reproduces, and [yyjson](https://github.com/ibireme/yyjson) (© YaoYuan, MIT) is
the parser it is built on. Both remain the property of their respective authors.
