"""Python ownership wrappers for LAE's high-level runtime API."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum
import os
from pathlib import Path
from typing import Final

from ._native import CProcessOptions, EncodedView, NativeLibrary, decode_view


LE_OK: Final = 0


class PrefixStrategy(IntEnum):
    PROPORTIONAL = 1
    FIXED = 2


class ReadingModel(IntEnum):
    PREFIX = 1
    LEXICAL_CORE = 2


class PresentationPolicy(IntEnum):
    BINARY = 1
    VARIABLE_STRENGTH = 2


class ModelType(IntEnum):
    PREFIX = 1
    LEXICAL_CORE = 2
    LINEAR_SALIENCE = 3


@dataclass(frozen=True)
class TextSpan:
    """A half-open range of UTF-8 byte offsets."""

    begin: int
    end: int


@dataclass(frozen=True)
class Emphasis:
    span: TextSpan
    strength: float
    style_class: int


@dataclass(frozen=True)
class ProcessOptions:
    language: str = ""
    prefix_strategy: PrefixStrategy = PrefixStrategy.PROPORTIONAL
    fixed_graphemes: int = 1
    prefix_proportion: float = 0.5
    emphasis_strength: float = 1.0
    presentation_policy: PresentationPolicy = PresentationPolicy.BINARY
    minimum_emphasis_strength: float = 0.0
    salience_threshold: float = 0.0
    reading_model: ReadingModel = ReadingModel.PREFIX


class LaeError(RuntimeError):
    def __init__(self, status: int, name: str, detail: str = "") -> None:
        self.status = status
        self.name = name
        self.detail = detail
        super().__init__(f"{name}: {detail}" if detail else name)


def _raise_status(native: NativeLibrary, runtime: ctypes.c_void_p, status: int) -> None:
    if status != LE_OK:
        raise LaeError(status, native.status_name(status), native.diagnostic(runtime))


class Model:
    """An immutable runtime model with an independently owned native handle."""

    def __init__(self, native: NativeLibrary, handle: ctypes.c_void_p) -> None:
        self._native = native
        self._handle: ctypes.c_void_p | None = handle

    def _open_handle(self) -> ctypes.c_void_p:
        if self._handle is None:
            raise RuntimeError("model is closed")
        return self._handle

    @property
    def type(self) -> ModelType:
        return ModelType(self._native.lib.le_model_type(self._open_handle()))

    @property
    def version(self) -> int:
        return int(self._native.lib.le_model_version(self._open_handle()))

    @property
    def minimum_abi(self) -> tuple[int, int]:
        packed = int(self._native.lib.le_model_minimum_abi_version(self._open_handle()))
        return packed >> 16, packed & 0xFFFF

    @property
    def languages(self) -> tuple[str, ...]:
        handle = self._open_handle()
        count = self._native.lib.le_model_language_count(handle)
        return tuple(
            decode_view(self._native.lib.le_model_language_at(handle, index))
            for index in range(count)
        )

    @property
    def required_features(self) -> tuple[int, ...]:
        handle = self._open_handle()
        count = self._native.lib.le_model_required_feature_count(handle)
        data = self._native.lib.le_model_required_feature_data(handle)
        return tuple(int(data[index]) for index in range(count))

    def supports_language(self, language: str) -> bool:
        encoded = EncodedView(language, ascii_only=True)
        return bool(
            self._native.lib.le_model_supports_language(self._open_handle(), encoded.view)
        )

    def close(self) -> None:
        if self._handle is not None:
            self._native.lib.le_model_destroy(self._handle)
            self._handle = None

    def __enter__(self) -> Model:
        self._open_handle()
        return self

    def __exit__(self, _type: object, _value: object, _traceback: object) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass


class Runtime:
    """A thread-safe LAE runtime; closing during another operation is unsupported."""

    def __init__(self, library: str | os.PathLike[str] | None = None) -> None:
        self._native = NativeLibrary(library)
        handle = ctypes.c_void_p()
        status = self._native.lib.le_runtime_create(None, ctypes.byref(handle))
        if status != LE_OK:
            raise LaeError(status, self._native.status_name(status))
        self._handle: ctypes.c_void_p | None = handle

    def _open_handle(self) -> ctypes.c_void_p:
        if self._handle is None:
            raise RuntimeError("runtime is closed")
        return self._handle

    def load_model(self, data: bytes | bytearray | memoryview) -> Model:
        runtime = self._open_handle()
        model_bytes = bytes(data)
        buffer = ctypes.create_string_buffer(model_bytes)
        handle = ctypes.c_void_p()
        status = self._native.lib.le_model_load(
            runtime, ctypes.cast(buffer, ctypes.c_void_p), len(model_bytes), ctypes.byref(handle)
        )
        _raise_status(self._native, runtime, status)
        return Model(self._native, handle)

    def load_model_file(self, path: str | os.PathLike[str]) -> Model:
        return self.load_model(Path(path).read_bytes())

    def process(
        self,
        text: str,
        options: ProcessOptions | None = None,
        *,
        model: Model | None = None,
    ) -> tuple[Emphasis, ...]:
        runtime = self._open_handle()
        if not isinstance(text, str):
            raise TypeError("text must be str")
        source = EncodedView(text)
        native_options = CProcessOptions()
        self._native.lib.le_process_options_init(ctypes.byref(native_options))
        configured = options if options is not None else ProcessOptions()
        language = EncodedView(configured.language, ascii_only=True)
        native_options.language = language.view
        native_options.prefix_strategy = int(configured.prefix_strategy)
        native_options.fixed_graphemes = configured.fixed_graphemes
        native_options.prefix_proportion = configured.prefix_proportion
        native_options.emphasis_strength = configured.emphasis_strength
        native_options.presentation_policy = int(configured.presentation_policy)
        native_options.minimum_emphasis_strength = configured.minimum_emphasis_strength
        native_options.salience_threshold = configured.salience_threshold
        native_options.reading_model = int(configured.reading_model)

        result = ctypes.c_void_p()
        if model is None:
            status = self._native.lib.le_process(
                runtime, source.view, ctypes.byref(native_options), ctypes.byref(result)
            )
        else:
            if model._native.identity != self._native.identity:
                raise ValueError("model and runtime were loaded from different shared libraries")
            status = self._native.lib.le_process_with_model(
                runtime,
                model._open_handle(),
                source.view,
                ctypes.byref(native_options),
                ctypes.byref(result),
            )
        _raise_status(self._native, runtime, status)
        try:
            count = self._native.lib.le_result_emphasis_count(result)
            data = self._native.lib.le_result_emphasis_data(result)
            return tuple(
                Emphasis(
                    TextSpan(int(data[index].span.begin), int(data[index].span.end)),
                    float(data[index].strength),
                    int(data[index].style_class),
                )
                for index in range(count)
            )
        finally:
            self._native.lib.le_result_destroy(result)

    def close(self) -> None:
        if self._handle is not None:
            self._native.lib.le_runtime_destroy(self._handle)
            self._handle = None

    def __enter__(self) -> Runtime:
        self._open_handle()
        return self

    def __exit__(self, _type: object, _value: object, _traceback: object) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass
