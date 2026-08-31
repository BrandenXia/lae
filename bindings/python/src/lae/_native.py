"""Private ctypes declarations for the LAE C ABI."""

from __future__ import annotations

import ctypes
import ctypes.util
import os
from pathlib import Path


class CStringView(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_size_t)]


class CTextSpan(ctypes.Structure):
    _fields_ = [("begin", ctypes.c_uint64), ("end", ctypes.c_uint64)]


class CEmphasis(ctypes.Structure):
    _fields_ = [
        ("span", CTextSpan),
        ("strength", ctypes.c_float),
        ("style_class", ctypes.c_uint32),
    ]


class CLanguageRegion(ctypes.Structure):
    _fields_ = [
        ("span", CTextSpan),
        ("language", CStringView),
        ("confidence", ctypes.c_float),
        ("reserved", ctypes.c_uint32),
    ]


class CProcessOptions(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("language", CStringView),
        ("prefix_strategy", ctypes.c_uint32),
        ("fixed_graphemes", ctypes.c_uint32),
        ("prefix_proportion", ctypes.c_float),
        ("emphasis_strength", ctypes.c_float),
        ("presentation_policy", ctypes.c_uint32),
        ("minimum_emphasis_strength", ctypes.c_float),
        ("salience_threshold", ctypes.c_float),
    ] + (
        [("reserved_v2", ctypes.c_uint32)]
        if ctypes.sizeof(ctypes.c_void_p) > ctypes.sizeof(ctypes.c_uint32)
        else []
    ) + [
        ("reading_model", ctypes.c_uint32),
        ("reserved_v3", ctypes.c_uint32),
    ]


class EncodedView:
    """Keep encoded bytes alive for the duration of a native call."""

    def __init__(self, value: str, *, ascii_only: bool = False) -> None:
        encoding = "ascii" if ascii_only else "utf-8"
        try:
            encoded = value.encode(encoding)
        except UnicodeEncodeError as error:
            raise ValueError("language must be an ASCII BCP-47 tag") from error
        self.buffer = ctypes.create_string_buffer(encoded)
        self.view = CStringView(ctypes.cast(self.buffer, ctypes.c_void_p), len(encoded))


def _library_name(explicit: str | os.PathLike[str] | None) -> str:
    if explicit is not None:
        return os.fspath(explicit)
    environment = os.environ.get("LAE_RUNTIME")
    if environment:
        return environment
    discovered = ctypes.util.find_library("le_runtime")
    if discovered:
        return discovered
    raise FileNotFoundError(
        "LAE shared runtime not found; pass its path to Runtime or set LAE_RUNTIME"
    )


class NativeLibrary:
    def __init__(self, explicit: str | os.PathLike[str] | None) -> None:
        name = _library_name(explicit)
        try:
            self.lib = ctypes.CDLL(name)
        except OSError as error:
            raise FileNotFoundError(f"could not load LAE shared runtime {name!r}: {error}") from error
        self.path = Path(name) if os.path.isabs(name) else name
        self.identity = os.path.realpath(name) if os.path.isabs(name) else name
        self._declare()

    def _declare(self) -> None:
        lib = self.lib
        required = (
            "le_runtime_create",
            "le_runtime_destroy",
            "le_process_options_init",
            "le_process",
            "le_process_with_model",
            "le_process_regions",
            "le_process_regions_with_model",
            "le_result_emphasis_count",
            "le_result_emphasis_data",
            "le_result_destroy",
            "le_model_load",
            "le_model_destroy",
            "le_model_type",
            "le_model_version",
            "le_model_minimum_abi_version",
            "le_model_language_count",
            "le_model_language_at",
            "le_model_supports_language",
            "le_model_required_feature_count",
            "le_model_required_feature_data",
            "le_runtime_last_error",
            "le_status_string",
        )
        missing = [name for name in required if not hasattr(lib, name)]
        if missing:
            raise RuntimeError(
                "shared library does not provide the required LAE ABI: " + ", ".join(missing)
            )

        lib.le_runtime_create.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
        lib.le_runtime_create.restype = ctypes.c_int32
        lib.le_runtime_destroy.argtypes = [ctypes.c_void_p]
        lib.le_runtime_destroy.restype = None

        lib.le_process_options_init.argtypes = [ctypes.POINTER(CProcessOptions)]
        lib.le_process_options_init.restype = None
        process_arguments = [
            ctypes.c_void_p,
            CStringView,
            ctypes.POINTER(CProcessOptions),
            ctypes.POINTER(ctypes.c_void_p),
        ]
        lib.le_process.argtypes = process_arguments
        lib.le_process.restype = ctypes.c_int32
        lib.le_process_with_model.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            CStringView,
            ctypes.POINTER(CProcessOptions),
            ctypes.POINTER(ctypes.c_void_p),
        ]
        lib.le_process_with_model.restype = ctypes.c_int32
        region_process_arguments = [
            ctypes.c_void_p,
            CStringView,
            ctypes.POINTER(CLanguageRegion),
            ctypes.c_size_t,
            ctypes.POINTER(CProcessOptions),
            ctypes.POINTER(ctypes.c_void_p),
        ]
        lib.le_process_regions.argtypes = region_process_arguments
        lib.le_process_regions.restype = ctypes.c_int32
        lib.le_process_regions_with_model.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            CStringView,
            ctypes.POINTER(CLanguageRegion),
            ctypes.c_size_t,
            ctypes.POINTER(CProcessOptions),
            ctypes.POINTER(ctypes.c_void_p),
        ]
        lib.le_process_regions_with_model.restype = ctypes.c_int32
        lib.le_result_emphasis_count.argtypes = [ctypes.c_void_p]
        lib.le_result_emphasis_count.restype = ctypes.c_size_t
        lib.le_result_emphasis_data.argtypes = [ctypes.c_void_p]
        lib.le_result_emphasis_data.restype = ctypes.POINTER(CEmphasis)
        lib.le_result_destroy.argtypes = [ctypes.c_void_p]
        lib.le_result_destroy.restype = None

        lib.le_model_load.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_void_p),
        ]
        lib.le_model_load.restype = ctypes.c_int32
        lib.le_model_destroy.argtypes = [ctypes.c_void_p]
        lib.le_model_destroy.restype = None
        for name in ("le_model_type", "le_model_version", "le_model_minimum_abi_version"):
            function = getattr(lib, name)
            function.argtypes = [ctypes.c_void_p]
            function.restype = ctypes.c_uint32
        lib.le_model_language_count.argtypes = [ctypes.c_void_p]
        lib.le_model_language_count.restype = ctypes.c_size_t
        lib.le_model_language_at.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        lib.le_model_language_at.restype = CStringView
        lib.le_model_supports_language.argtypes = [ctypes.c_void_p, CStringView]
        lib.le_model_supports_language.restype = ctypes.c_int
        lib.le_model_required_feature_count.argtypes = [ctypes.c_void_p]
        lib.le_model_required_feature_count.restype = ctypes.c_size_t
        lib.le_model_required_feature_data.argtypes = [ctypes.c_void_p]
        lib.le_model_required_feature_data.restype = ctypes.POINTER(ctypes.c_uint32)

        lib.le_runtime_last_error.argtypes = [ctypes.c_void_p]
        lib.le_runtime_last_error.restype = CStringView
        lib.le_status_string.argtypes = [ctypes.c_int32]
        lib.le_status_string.restype = ctypes.c_char_p

    def status_name(self, status: int) -> str:
        value = self.lib.le_status_string(status)
        return value.decode("ascii") if value else "LE_ERROR_UNKNOWN"

    def diagnostic(self, runtime: ctypes.c_void_p) -> str:
        view = self.lib.le_runtime_last_error(runtime)
        if not view.data or not view.size:
            return ""
        return ctypes.string_at(view.data, view.size).decode("utf-8", errors="replace")


def decode_view(view: CStringView) -> str:
    if not view.data or not view.size:
        return ""
    return ctypes.string_at(view.data, view.size).decode("utf-8")
