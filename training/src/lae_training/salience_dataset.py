"""Provider-neutral feature snapshots for learned salience models."""

from __future__ import annotations

import math
import struct
from collections.abc import Iterable, Iterator, Mapping
from dataclasses import dataclass
from pathlib import Path

from .dataset import DatasetError
from .evaluation_records import identifier, integer, records, score


SCHEMA_VERSION = 1
KNOWN_FEATURE_IDS = frozenset(
    {
        0x00000001,
        0x00000002,
        0x00000003,
        0x00010001,
        0x00010002,
        0x00010003,
        0x00030001,
        0x00040001,
        0x00040002,
    }
)


def binary32(value: float, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise DatasetError(f"{field} must be a number")
    try:
        result = struct.unpack("<f", struct.pack("<f", value))[0]
    except (OverflowError, struct.error) as error:
        raise DatasetError(f"{field} must fit IEEE-754 binary32") from error
    if not math.isfinite(result):
        raise DatasetError(f"{field} must be finite")
    return result


@dataclass(frozen=True, slots=True)
class SalienceFeature:
    feature_id: int
    value: float


@dataclass(frozen=True, slots=True)
class SalienceUnit:
    features: tuple[SalienceFeature, ...]
    target_salience: float

    def feature_value(self, feature_id: int) -> float:
        for feature in self.features:
            if feature.feature_id == feature_id:
                return feature.value
        return 0.0


@dataclass(frozen=True, slots=True)
class SalienceExample:
    identifier: str
    language: str
    units: tuple[SalienceUnit, ...]


def _language_is_valid(language: str) -> bool:
    parts = language.split("-")
    return (
        0 < len(language) <= 255
        and language.isascii()
        and all(part and part.isalnum() for part in parts)
    )


def parse_salience_record(
    record: Mapping[str, object], source: str = "record"
) -> SalienceExample:
    if integer(record.get("schema_version"), f"{source}.schema_version") != SCHEMA_VERSION:
        raise DatasetError(f"{source}.schema_version is not supported")
    example_id = identifier(record.get("id"), f"{source}.id")
    language = identifier(record.get("language"), f"{source}.language")
    if not _language_is_valid(language):
        raise DatasetError(f"{source}.language must be a BCP-47-compatible ASCII tag")
    raw_units = record.get("units")
    if not isinstance(raw_units, list) or not raw_units:
        raise DatasetError(f"{source}.units must be a nonempty array")
    units: list[SalienceUnit] = []
    for unit_index, raw_unit in enumerate(raw_units):
        unit_source = f"{source}.units[{unit_index}]"
        if not isinstance(raw_unit, Mapping):
            raise DatasetError(f"{unit_source} must be an object")
        target = score(raw_unit.get("target_salience"), f"{unit_source}.target_salience")
        raw_features = raw_unit.get("features")
        if not isinstance(raw_features, list) or not raw_features:
            raise DatasetError(f"{unit_source}.features must be a nonempty array")
        features: list[SalienceFeature] = []
        seen: set[int] = set()
        for feature_index, raw_feature in enumerate(raw_features):
            feature_source = f"{unit_source}.features[{feature_index}]"
            if not isinstance(raw_feature, Mapping):
                raise DatasetError(f"{feature_source} must be an object")
            feature_id = integer(raw_feature.get("id"), f"{feature_source}.id", minimum=0)
            if feature_id not in KNOWN_FEATURE_IDS:
                raise DatasetError(f"{feature_source}.id is not supported by the runtime")
            if feature_id in seen:
                raise DatasetError(f"{unit_source} contains duplicate feature {feature_id}")
            seen.add(feature_id)
            features.append(
                SalienceFeature(
                    feature_id,
                    binary32(raw_feature.get("value"), f"{feature_source}.value"),
                )
            )
        units.append(SalienceUnit(tuple(features), target))
    return SalienceExample(example_id, language, tuple(units))


class SalienceJsonlDataset(Iterable[SalienceExample]):
    def __init__(self, path: str | Path):
        self.path = Path(path)

    def __iter__(self) -> Iterator[SalienceExample]:
        seen: set[str] = set()
        for value, source in records(self.path):
            try:
                example = parse_salience_record(value, source)
            except ValueError as error:
                if isinstance(error, DatasetError):
                    raise
                raise DatasetError(str(error)) from error
            if example.identifier in seen:
                raise DatasetError(f"{source} repeats id {example.identifier!r}")
            seen.add(example.identifier)
            yield example
