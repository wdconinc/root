#!/usr/bin/env python3
"""
gen_wasm_bindings.py  —  Generate EMSCRIPTEN_BINDINGS from ROOT class headers.

Reads a YAML manifest that lists classes and their headers, uses libclang to
parse the headers, and emits EMSCRIPTEN_BINDINGS() blocks for each class.

Usage:
  python3 gen_wasm_bindings.py \\
      --manifest core/wasm/bindings.yml \\
      --output   core/wasm/src/RootWasmBindings_generated.cxx \\
      --root-src /path/to/root-source

Requirements:
  pip install libclang pyyaml
"""

import argparse
import re
import sys
import textwrap
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("pyyaml not found — run: pip install pyyaml")

try:
    import clang.cindex as clang
except ImportError:
    sys.exit("libclang not found — run: pip install libclang")

# ─── Type mapping ────────────────────────────────────────────────────────────

# C++/ROOT scalar types that Embind can bind directly to JS number/bool
_SCALAR_TYPES = {
    "void", "bool",
    "char", "unsigned char", "signed char",
    "short", "unsigned short",
    "int", "unsigned int",
    "long", "unsigned long",
    "long long", "unsigned long long",
    "float", "double", "long double",
    # ROOT typedefs
    "Int_t", "UInt_t", "Long_t", "ULong_t", "Long64_t", "ULong64_t",
    "Float_t", "Double_t", "Bool_t", "Char_t", "Short_t", "Byte_t",
    "Version_t", "Option_t",
}

# Methods to skip regardless of class
_SKIP_METHODS = {
    # ROOT reflection/streaming boilerplate
    "IsA", "ShowMembers", "Streamer", "StreamerNVirtual",
    "CheckTObjectHashConsistency", "Class", "Class_Name", "Class_Version",
    "DeclFileName", "ImplFileName", "DeclFileLine", "ImplFileLine",
    # Operators that Embind cannot easily bind
    "operator=", "operator==", "operator!=", "operator<", "operator>",
    "operator<=", "operator>=", "operator+", "operator-", "operator*",
    "operator/", "operator[]", "operator()",
    "operator new", "operator delete", "operator new[]", "operator delete[]",
}


def _strip_type(spelling: str) -> str:
    """Reduce a type spelling to its bare name for lookup."""
    return (spelling.replace("const", "")
                    .replace("&", "")
                    .replace("*", "")
                    .strip())


def _is_bindable_type(spelling: str) -> bool:
    """Return True if Embind can handle this type without custom wrappers."""
    s = spelling.strip()
    if s == "void":
        return True
    # const char* is supported (Embind converts to/from JS string)
    if s in ("const char *", "const char*"):
        return True
    # Any other raw pointer needs allow_raw_pointers() — skip
    if "*" in s:
        return False
    # Strip const and reference qualifiers, then check scalar types
    s = s.replace("const", "").replace("&", "").strip()
    if s in _SCALAR_TYPES:
        return True
    # std::string is supported by Embind natively
    if s in ("std::string", "string"):
        return True
    return False


def _needs_string_wrapper(spelling: str) -> bool:
    """Return True if the type is a const char* that we should wrap as std::string."""
    return spelling.strip() in ("const char *", "const char*")


# ─── Clang parsing helpers ────────────────────────────────────────────────────

def _find_class_cursor(tu, class_name: str):
    """Walk the translation unit and find the class/struct definition."""
    def _walk(cursor):
        try:
            kind = cursor.kind
        except ValueError:
            return None  # unknown cursor kind — skip
        if kind in (clang.CursorKind.CLASS_DECL, clang.CursorKind.STRUCT_DECL):
            if cursor.spelling == class_name and cursor.is_definition():
                return cursor
        for child in cursor.get_children():
            result = _walk(child)
            if result:
                return result
        return None

    return _walk(tu.cursor)


def _get_base_classes(cursor):
    """Return list of (base_class_name, is_public) for direct bases."""
    bases = []
    for child in cursor.get_children():
        try:
            kind = child.kind
        except ValueError:
            continue
        if kind == clang.CursorKind.CXX_BASE_SPECIFIER:
            is_public = child.access_specifier == clang.AccessSpecifier.PUBLIC
            name = child.spelling.replace("class ", "").replace("struct ", "").strip()
            bases.append((name, is_public))
    return bases


def _get_public_constructors(cursor, class_name: str):
    """Return list of unique constructor parameter lists (list of type spellings).
    Deduplicates by arity — prefers double over int (libclang sometimes misresolves
    Double_t default-arg constructors to int)."""
    candidates = []
    for child in cursor.get_children():
        try:
            kind = child.kind
        except ValueError:
            continue
        if kind != clang.CursorKind.CONSTRUCTOR:
            continue
        if child.access_specifier != clang.AccessSpecifier.PUBLIC:
            continue
        if child.spelling.startswith("~"):
            continue
        params = [arg.type.spelling for arg in child.get_arguments()]
        # Skip pointer params (need allow_raw_pointers)
        if any("*" in p for p in params):
            continue
        # Skip any reference parameters — libclang sometimes misresolves complex types
        # (TVectorD, TH1, etc.) to "const int &" or similar.
        if any("&" in p for p in params):
            continue
        # Only include if all param types are scalar
        if not all(_is_bindable_type(p) for p in params):
            continue
        candidates.append(params)

    # Deduplicate by arity — prefer float/double params over int (correct ROOT types)
    by_arity: dict = {}
    for params in candidates:
        arity = len(params)
        if arity not in by_arity:
            by_arity[arity] = params
        else:
            prev = by_arity[arity]
            def _float_score(ps):
                return sum(1 for p in ps if "double" in p or "float" in p
                           or "Double_t" in p or "Float_t" in p)
            if _float_score(params) > _float_score(prev):
                by_arity[arity] = params

    return list(by_arity.values())


def _get_public_methods(cursor):
    """Return list of (method_name, return_spelling, [(param_name, type_spelling)], is_const).
    Overloaded methods are skipped — plain &Class::Method is ambiguous for the compiler."""
    # First pass: count how many public overloads each method name has
    overload_count: dict = {}
    for child in cursor.get_children():
        try:
            kind = child.kind
        except ValueError:
            continue
        if kind != clang.CursorKind.CXX_METHOD:
            continue
        if child.access_specifier != clang.AccessSpecifier.PUBLIC:
            continue
        name = child.spelling
        if name in _SKIP_METHODS or name.startswith("~"):
            continue
        overload_count[name] = overload_count.get(name, 0) + 1

    methods = []
    seen = set()

    for child in cursor.get_children():
        try:
            kind = child.kind
        except ValueError:
            continue
        if kind != clang.CursorKind.CXX_METHOD:
            continue
        if child.access_specifier != clang.AccessSpecifier.PUBLIC:
            continue
        name = child.spelling
        if name in _SKIP_METHODS or name.startswith("~"):
            continue
        if name in seen:
            continue
        # Skip overloaded methods — &Class::Method is ambiguous
        if overload_count.get(name, 1) > 1:
            continue

        ret = child.result_type.spelling
        params = [(arg.spelling or f"arg{i}", arg.type.spelling)
                  for i, arg in enumerate(child.get_arguments())]

        # Cross-check: libclang sometimes returns 0 arg cursors for methods whose
        # parameter types it cannot resolve (e.g. Double_t* arrays).  If the
        # function type reports more arguments than get_arguments() returned, the
        # parameter-type filter would pass vacuously and produce a broken binding.
        try:
            declared_arg_count = child.type.get_num_arg_types()
        except Exception:
            declared_arg_count = len(params)
        if declared_arg_count != len(params):
            continue  # libclang couldn't resolve some param types → skip

        if not _is_bindable_type(ret):
            continue
        if not all(_is_bindable_type(ptype) for _, ptype in params):
            continue

        methods.append((name, ret, params, child.is_const_method()))
        seen.add(name)

    return methods


# ─── Code emitter ─────────────────────────────────────────────────────────────

def _emit_class_bindings(class_info: dict, cursor, tu_includes: list) -> str:
    """Generate a EMSCRIPTEN_BINDINGS block for one class."""
    name = class_info["name"]
    explicit_base = class_info.get("base")  # from manifest
    skip_methods = set(class_info.get("skip_methods", []))
    extra_methods = class_info.get("extra_methods", [])  # raw strings

    bases = _get_base_classes(cursor)
    ctors = _get_public_constructors(cursor, name)
    methods = [m for m in _get_public_methods(cursor) if m[0] not in skip_methods]

    # Determine the base to use in Embind (prefer manifest override)
    embind_base = explicit_base
    if embind_base is None and bases:
        # Use first public base
        for bname, bpub in bases:
            if bpub:
                embind_base = bname
                break

    # Parse extra_methods: separate .constructor<...>() entries from .function(...) entries
    extra_ctors = [e for e in extra_methods if ".constructor" in e]
    extra_funcs = [e for e in extra_methods if ".constructor" not in e]

    # If extra_ctors are provided, suppress auto-detected constructors with the same arity
    # (extra_ctors are authoritative when libclang misresolves param types, e.g. Double_t→int)
    extra_ctor_arities = set()
    for ec in extra_ctors:
        # Extract arity from ".constructor<A, B, C>()" → 3
        inner = re.search(r'\.constructor<([^>]*)>', ec)
        if inner:
            types_str = inner.group(1).strip()
            arity = len(types_str.split(",")) if types_str else 0
            extra_ctor_arities.add(arity)
    ctors = [p for p in ctors if len(p) not in extra_ctor_arities]

    # Build set of method names already covered by auto-detection or skip
    auto_method_names = {m[0] for m in methods}
    # Collect method names already provided as extra (to suppress duplicates in auto list)
    extra_func_names = set()
    for ef in extra_funcs:
        m = re.search(r'\.function\("([^"]+)"', ef)
        if m:
            extra_func_names.add(m.group(1))
    # Filter auto methods to avoid duplicating what extra_funcs already provides
    methods = [m for m in methods if m[0] not in extra_func_names]

    lines = []
    lines.append(f"EMSCRIPTEN_BINDINGS({name}_gen_bindings)")
    lines.append("{")

    if embind_base:
        lines.append(f'  class_<{name}, base<{embind_base}>>("{name}")')
    else:
        lines.append(f'  class_<{name}>("{name}")')

    # Auto-detected constructors
    for params in ctors:
        if not params:
            lines.append("    .constructor<>()")
        else:
            ptypes = ", ".join(params)
            lines.append(f"    .constructor<{ptypes}>()")

    # Extra constructors from manifest (appended after auto ones)
    for ec in extra_ctors:
        lines.append(f"    {ec.strip()}")

    # Auto-detected methods
    for mname, ret, params, is_const in methods:
        lines.append(f'    .function("{mname}", &{name}::{mname})')

    # Extra methods from manifest
    for ef in extra_funcs:
        lines.append(f"    {ef.strip()}")

    lines.append("    ;")
    lines.append("}")

    return "\n".join(lines)


# ─── Main ─────────────────────────────────────────────────────────────────────

def _build_index(root_src: Path) -> clang.Index:
    return clang.Index.create()


def _include_args(root_src: Path, extra_includes: list) -> list:
    """Build -I flags for the ROOT source tree."""
    args = ["-x", "c++", "-std=c++17",
            "-DEMSCRIPTEN",      # pretend we're compiling for WASM
            "-D__EMSCRIPTEN__",
            "-w"]               # suppress warnings during parsing

    # Core ROOT include paths (in-source)
    standard_dirs = [
        "core/base/inc",
        "core/cont/inc",
        "core/meta/inc",
        "core/zip/inc",
        "hist/hist/inc",
        "math/mathcore/inc",
        "math/physics/inc",
        "io/io/inc",
        "tree/tree/inc",
        "graf2d/graf/inc",
        "graf2d/gpad/inc",
    ]
    for d in standard_dirs:
        p = root_src / d
        if p.is_dir():
            args += ["-I", str(p)]

    for inc in extra_includes:
        args += ["-I", inc]

    return args


def generate(manifest_path: Path, output_path: Path, root_src: Path):
    with open(manifest_path) as f:
        manifest = yaml.safe_load(f)

    extra_includes = manifest.get("include_dirs", [])
    classes = manifest.get("classes", [])

    index = _build_index(root_src)
    args = _include_args(root_src, extra_includes)

    output_lines = [
        "// AUTO-GENERATED by gen_wasm_bindings.py — DO NOT EDIT.",
        "// Regenerate: python3 core/wasm/tools/gen_wasm_bindings.py \\",
        "//   --manifest core/wasm/bindings.yml \\",
        "//   --output   core/wasm/src/RootWasmBindings_generated.cxx",
        "",
        "#ifdef __EMSCRIPTEN__",
        "#include <emscripten/bind.h>",
        "using namespace emscripten;",
        "",
    ]

    # Collect headers (unique, in manifest order)
    seen_headers = []
    for cls in classes:
        hdr = cls["header"]
        if hdr not in seen_headers:
            seen_headers.append(hdr)

    # Find and emit #includes
    for hdr in seen_headers:
        output_lines.append(f'#include "{hdr}"')
    output_lines.append("")

    errors = []
    for cls in classes:
        name = cls["name"]
        header = cls["header"]

        # Locate header file
        header_path = None
        for d in args:
            if d.startswith("-I"):
                continue
            candidate = Path(d) / header if not d.startswith("-") else None
        # Search the include dirs we built
        inc_dirs = []
        it = iter(args)
        for a in it:
            if a == "-I":
                inc_dirs.append(next(it))
        for d in inc_dirs:
            candidate = Path(d) / header
            if candidate.exists():
                header_path = candidate
                break

        if header_path is None:
            errors.append(f"  // WARNING: header {header!r} not found for {name}")
            continue

        # Parse
        tu = index.parse(str(header_path), args=args)
        cursor = _find_class_cursor(tu, name)
        if cursor is None:
            errors.append(f"  // WARNING: class {name!r} not found in {header}")
            continue

        block = _emit_class_bindings(cls, cursor, args)
        output_lines.append(f"// ── {name} " + "─" * max(0, 68 - len(name)))
        output_lines.append(block)
        output_lines.append("")

    if errors:
        output_lines.append("// Generation warnings:")
        output_lines.extend(errors)
        output_lines.append("")

    output_lines.append("#endif  // __EMSCRIPTEN__")

    output_path.write_text("\n".join(output_lines) + "\n")
    print(f"Generated {len(classes)} class bindings → {output_path}")
    if errors:
        print(f"  ({len(errors)} warnings — see generated file)")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--manifest", required=True, type=Path,
                    help="Path to bindings.yml manifest")
    ap.add_argument("--output", required=True, type=Path,
                    help="Output .cxx file path")
    ap.add_argument("--root-src", type=Path, default=Path("."),
                    help="Root of ROOT source tree (default: cwd)")
    args = ap.parse_args()

    generate(args.manifest, args.output, args.root_src)


if __name__ == "__main__":
    main()
