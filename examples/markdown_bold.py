#!/usr/bin/env python3
"""Register a tiny Python provider and render LAE output as Markdown bold."""

import argparse
import ctypes
import os
from pathlib import Path
import re
import sys

LE_OK = 0
LE_ERROR_PLUGIN_FAILURE = -7
LE_PROVIDER_ABI_V1 = 1 << 16
LE_NODE_DOCUMENT = 1
LE_NODE_UNIT = 5


class StringView(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_size_t)]


class TextSpan(ctypes.Structure):
    _fields_ = [("begin", ctypes.c_uint64), ("end", ctypes.c_uint64)]


class Emphasis(ctypes.Structure):
    _fields_ = [("span", TextSpan), ("strength", ctypes.c_float), ("style", ctypes.c_uint32)]


AddNode = ctypes.CFUNCTYPE(
    ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, TextSpan
)
AddChild = ctypes.CFUNCTYPE(
    ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32
)
AddRegion = ctypes.CFUNCTYPE(
    ctypes.c_int32, ctypes.c_void_p, TextSpan, StringView, ctypes.c_float
)


class AnalysisSink(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("context", ctypes.c_void_p),
        ("add_node", AddNode),
        ("add_child", AddChild),
        ("add_feature", ctypes.c_void_p),
        ("add_language_region", AddRegion),
    ]


Supports = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, StringView)
Analyze = ctypes.CFUNCTYPE(
    ctypes.c_int32, ctypes.c_void_p, StringView, StringView, ctypes.POINTER(AnalysisSink)
)


class Provider(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
        ("name", StringView),
        ("context", ctypes.c_void_p),
        ("supports", Supports),
        ("analyze", Analyze),
        ("destroy", ctypes.c_void_p),
    ]


class ProcessOptions(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("language", StringView),
        ("prefix_strategy", ctypes.c_uint32),
        ("fixed_graphemes", ctypes.c_uint32),
        ("prefix_proportion", ctypes.c_float),
        ("emphasis_strength", ctypes.c_float),
        ("presentation_policy", ctypes.c_uint32),
        ("minimum_emphasis_strength", ctypes.c_float),
        ("salience_threshold", ctypes.c_float),
    ] + ([("reserved_v2", ctypes.c_uint32)] if ctypes.sizeof(ctypes.c_void_p) > 4 else []) + [
        ("reading_model", ctypes.c_uint32),
        ("reserved_v3", ctypes.c_uint32),
    ]


def as_view(buffer):
    return StringView(ctypes.cast(buffer, ctypes.c_void_p), len(buffer.raw) - 1)


def find_runtime(explicit):
    if explicit:
        return Path(explicit).expanduser().resolve()
    build = Path(__file__).resolve().parent.parent / "build"
    for name in ("lible_runtime.dylib", "lible_runtime.so", "le_runtime.dll"):
        matches = sorted(
            build.glob(f"**/{name}"), key=lambda path: path.stat().st_mtime, reverse=True
        )
        for candidate in matches:
            try:
                getattr(ctypes.CDLL(str(candidate)), "le_runtime_register_provider")
                return candidate
            except (AttributeError, OSError):
                pass
    raise SystemExit(
        "No shared runtime found. Run:\n"
        "  cmake -S . -B build -DBUILD_SHARED_LIBS=ON && cmake --build build"
    )


def load_runtime(path):
    lib = ctypes.CDLL(str(path))
    if not hasattr(lib, "le_runtime_register_provider"):
        raise SystemExit(f"{path} predates the provider ABI; rebuild the shared runtime")
    lib.le_runtime_create.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    lib.le_runtime_create.restype = ctypes.c_int32
    lib.le_runtime_destroy.argtypes = [ctypes.c_void_p]
    lib.le_runtime_register_provider.argtypes = [ctypes.c_void_p, ctypes.POINTER(Provider)]
    lib.le_runtime_register_provider.restype = ctypes.c_int32
    lib.le_process_options_init.argtypes = [ctypes.POINTER(ProcessOptions)]
    lib.le_process.argtypes = [
        ctypes.c_void_p, StringView, ctypes.POINTER(ProcessOptions), ctypes.POINTER(ctypes.c_void_p)
    ]
    lib.le_process.restype = ctypes.c_int32
    lib.le_result_emphasis_count.argtypes = [ctypes.c_void_p]
    lib.le_result_emphasis_count.restype = ctypes.c_size_t
    lib.le_result_emphasis_data.argtypes = [ctypes.c_void_p]
    lib.le_result_emphasis_data.restype = ctypes.POINTER(Emphasis)
    lib.le_result_destroy.argtypes = [ctypes.c_void_p]
    lib.le_status_string.argtypes = [ctypes.c_int32]
    lib.le_status_string.restype = ctypes.c_char_p
    return lib


def make_provider():
    @Supports
    def supports(_context, language):
        return ctypes.string_at(language.data, language.size) == b"demo"

    @Analyze
    def analyze(_context, text, language, sink_pointer):
        try:
            source = ctypes.string_at(text.data, text.size)
            decoded = source.decode("utf-8")
            offsets = [0]
            for character in decoded:
                offsets.append(offsets[-1] + len(character.encode("utf-8")))
            units = [
                (offsets[item.start()], offsets[item.end()])
                for item in re.finditer(r"\S+", decoded)
            ]
            sink = sink_pointer.contents
            status = sink.add_node(sink.context, 0, LE_NODE_DOCUMENT, TextSpan(0, len(source)))
            for node_id, (begin, end) in enumerate(units, 1):
                if status == LE_OK:
                    status = sink.add_node(
                        sink.context, node_id, LE_NODE_UNIT, TextSpan(begin, end)
                    )
            for node_id in range(1, len(units) + 1):
                if status == LE_OK:
                    status = sink.add_child(sink.context, 0, node_id)
            if status == LE_OK:
                status = sink.add_language_region(
                    sink.context, TextSpan(0, len(source)), language, 1.0
                )
            return status
        except Exception:
            return LE_ERROR_PLUGIN_FAILURE

    name = ctypes.create_string_buffer(b"python-demo")
    descriptor = Provider(
        ctypes.sizeof(Provider),
        LE_PROVIDER_ABI_V1,
        0,
        0,
        as_view(name),
        None,
        supports,
        analyze,
        None,
    )
    return descriptor, supports, analyze, name  # Keep callbacks and name alive.


def render(source, spans):
    output = bytearray()
    cursor = 0
    for begin, end in spans:
        output += source[cursor:begin] + b"**" + source[begin:end] + b"**"
        cursor = end
    return bytes(output + source[cursor:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paragraph", nargs="?", help="text; reads stdin when omitted")
    parser.add_argument("--library", default=os.environ.get("LAE_RUNTIME"))
    args = parser.parse_args()
    paragraph = args.paragraph if args.paragraph is not None else sys.stdin.read().rstrip("\n")
    source = paragraph.encode("utf-8")
    text = ctypes.create_string_buffer(source)
    language = ctypes.create_string_buffer(b"demo")
    runtime = load_runtime(find_runtime(args.library))
    handle, result = ctypes.c_void_p(), ctypes.c_void_p()
    status = runtime.le_runtime_create(None, ctypes.byref(handle))
    if status != LE_OK:
        raise SystemExit(runtime.le_status_string(status).decode())
    provider = make_provider()
    try:
        status = runtime.le_runtime_register_provider(handle, ctypes.byref(provider[0]))
        options = ProcessOptions()
        runtime.le_process_options_init(ctypes.byref(options))
        options.language = as_view(language)
        if status == LE_OK:
            status = runtime.le_process(
                handle, as_view(text), ctypes.byref(options), ctypes.byref(result)
            )
        if status != LE_OK:
            raise SystemExit(runtime.le_status_string(status).decode())
        data = runtime.le_result_emphasis_data(result)
        spans = [
            (data[index].span.begin, data[index].span.end)
            for index in range(runtime.le_result_emphasis_count(result))
        ]
        sys.stdout.buffer.write(render(source, spans) + b"\n")
    finally:
        runtime.le_result_destroy(result)
        runtime.le_runtime_destroy(handle)
        _ = provider


if __name__ == "__main__":
    main()
