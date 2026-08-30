"""Pure-Python compiler for the portable LAE model artifact format v1."""

from __future__ import annotations

import math
import struct
import zlib
from collections.abc import Iterable
from pathlib import Path

from .evaluation import PrefixCandidate


MAGIC = b"LAEMODL\0"
FORMAT_VERSION = (1, 0)
HEADER_SIZE = 64
MAXIMUM_ARTIFACT_SIZE = 16 * 1024 * 1024
ABI_VERSION = (1 << 16) | 6
MODEL_PREFIX = 1
MODEL_LEXICAL_CORE = 2
MODEL_LINEAR_SALIENCE = 3
LINEAR_SALIENCE_MINIMUM_ABI = (1 << 16) | 6
FEATURE_LEXICAL_CORE = 0x00010001
PREFIX_PROPORTIONAL = 1
PREFIX_FIXED = 2


def _u32(value: int, field: str, *, nonzero: bool = False) -> int:
    if type(value) is not int or value < int(nonzero) or value > 0xFFFFFFFF:
        qualifier = "nonzero " if nonzero else ""
        raise ValueError(f"{field} must be a {qualifier}uint32")
    return value


def _language_is_valid(language: str) -> bool:
    ascii_size = len(language.encode("ascii", errors="ignore"))
    if not language or ascii_size != len(language) or len(language) > 255:
        return False
    parts = language.split("-")
    return all(part and part.isalnum() and part.isascii() for part in parts)


def _languages(values: Iterable[str]) -> tuple[str, ...]:
    result: list[str] = []
    folded: set[str] = set()
    for value in values:
        if not _language_is_valid(value):
            raise ValueError(f"invalid model language tag: {value!r}")
        key = value.lower()
        if key in folded:
            raise ValueError(f"duplicate model language tag: {value!r}")
        folded.add(key)
        result.append(value)
    if len(result) > 64:
        raise ValueError("model language count exceeds 64")
    return tuple(result)


def _binary32(value: float, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{field} must be a number")
    try:
        result = struct.unpack("<f", struct.pack("<f", value))[0]
    except (OverflowError, struct.error) as error:
        raise ValueError(f"{field} must fit IEEE-754 binary32") from error
    if not math.isfinite(result):
        raise ValueError(f"{field} must be finite")
    return result


def _encode(
    *,
    model_type: int,
    model_version: int,
    minimum_abi: int,
    languages: Iterable[str],
    required_features: tuple[int, ...],
    parameters: bytes,
) -> bytes:
    model_version = _u32(model_version, "model_version", nonzero=True)
    minimum_abi = _u32(minimum_abi, "minimum_abi")
    language_values = _languages(languages)
    if len(required_features) > 256 or len(set(required_features)) != len(required_features):
        raise ValueError("required features must be unique and contain at most 256 entries")
    language_table = b"".join(
        struct.pack("<H", len(language.encode("ascii"))) + language.encode("ascii")
        for language in language_values
    )
    feature_table = b"".join(
        struct.pack("<I", _u32(value, "required feature")) for value in required_features
    )
    feature_offset = HEADER_SIZE + len(language_table)
    parameter_offset = feature_offset + len(feature_table)
    total_size = parameter_offset + len(parameters)
    if len(parameters) % 4 != 0 or total_size > MAXIMUM_ARTIFACT_SIZE:
        raise ValueError("artifact parameter layout or total size is invalid")
    header = struct.pack(
        "<8sHH13I",
        MAGIC,
        FORMAT_VERSION[0],
        FORMAT_VERSION[1],
        HEADER_SIZE,
        total_size,
        0,
        minimum_abi,
        _u32(model_type, "model_type"),
        model_version,
        len(language_values),
        len(required_features),
        HEADER_SIZE,
        feature_offset,
        parameter_offset,
        len(parameters) // 4,
        0,
    )
    artifact = bytearray(header + language_table + feature_table + parameters)
    struct.pack_into("<I", artifact, 20, zlib.crc32(artifact) & 0xFFFFFFFF)
    return bytes(artifact)


def build_prefix_artifact(
    candidate: PrefixCandidate,
    *,
    languages: Iterable[str] = (),
    model_version: int = 1,
    minimum_abi: int = ABI_VERSION,
) -> bytes:
    """Compile fitted prefix parameters into a runtime-loadable artifact."""

    strategy = PREFIX_FIXED if candidate.strategy == "fixed" else PREFIX_PROPORTIONAL
    if not math.isfinite(candidate.proportion):
        raise ValueError("prefix proportion must be finite")
    parameters = struct.pack(
        "<IIf", strategy, candidate.fixed_graphemes, candidate.proportion
    )
    return _encode(
        model_type=MODEL_PREFIX,
        model_version=model_version,
        minimum_abi=minimum_abi,
        languages=languages,
        required_features=(),
        parameters=parameters,
    )


def build_lexical_core_artifact(
    *,
    languages: Iterable[str] = (),
    model_version: int = 1,
    minimum_abi: int = ABI_VERSION,
) -> bytes:
    """Compile a lexical-core model descriptor for the current runtime."""

    return _encode(
        model_type=MODEL_LEXICAL_CORE,
        model_version=model_version,
        minimum_abi=minimum_abi,
        languages=languages,
        required_features=(FEATURE_LEXICAL_CORE,),
        parameters=b"",
    )


def build_linear_salience_artifact(
    bias: float,
    weights: Iterable[tuple[int, float]],
    *,
    languages: Iterable[str] = (),
    model_version: int = 1,
    minimum_abi: int = ABI_VERSION,
) -> bytes:
    """Compile a linear unit-salience predictor into artifact format v1."""

    minimum_abi = _u32(minimum_abi, "minimum_abi")
    if minimum_abi < LINEAR_SALIENCE_MINIMUM_ABI:
        raise ValueError("linear model minimum ABI must be at least 1.6")
    bias = _binary32(bias, "linear bias")
    values: list[tuple[int, float]] = []
    seen: set[int] = set()
    for feature, weight in weights:
        feature = _u32(feature, "linear feature")
        if feature in seen:
            raise ValueError(f"duplicate linear feature: {feature}")
        seen.add(feature)
        values.append((feature, _binary32(weight, f"weight for feature {feature}")))
    if not values or len(values) > 256:
        raise ValueError("linear model must contain between 1 and 256 weights")
    parameters = struct.pack("<fI", bias, len(values)) + b"".join(
        struct.pack("<If", feature, weight) for feature, weight in values
    )
    return _encode(
        model_type=MODEL_LINEAR_SALIENCE,
        model_version=model_version,
        minimum_abi=minimum_abi,
        languages=languages,
        required_features=tuple(feature for feature, _ in values),
        parameters=parameters,
    )


def write_artifact(path: str | Path, artifact: bytes) -> None:
    """Write one already-validated artifact without retaining training state."""

    Path(path).write_bytes(artifact)
