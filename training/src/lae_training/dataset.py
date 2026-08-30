"""Versioned, runtime-independent training dataset contracts."""

from __future__ import annotations

import json
from collections.abc import Iterable, Iterator, Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


SCHEMA_VERSION = 1


class DatasetError(ValueError):
    """Raised when a training record violates the dataset contract."""


@dataclass(frozen=True, slots=True)
class UnitTarget:
    """One linguistic unit and its target emphasized grapheme count."""

    begin: int
    end: int
    grapheme_boundaries: tuple[int, ...]
    target_prefix_graphemes: int

    @property
    def grapheme_count(self) -> int:
        return len(self.grapheme_boundaries) - 1


@dataclass(frozen=True, slots=True)
class TrainingExample:
    """An immutable text example using absolute UTF-8 byte offsets."""

    identifier: str
    text: str
    language: str
    units: tuple[UnitTarget, ...]


class Dataset(Protocol):
    """Minimal repeatable dataset interface consumed by training stages."""

    def __iter__(self) -> Iterator[TrainingExample]: ...


def _integer(value: object, field: str) -> int:
    if type(value) is not int:
        raise DatasetError(f"{field} must be an integer")
    return value


def _language_is_valid(language: str) -> bool:
    if not language or len(language) > 255:
        return False
    previous_hyphen = True
    for character in language:
        hyphen = character == "-"
        if (not hyphen and not character.isascii()) or (
            not hyphen and not character.isalnum()
        ):
            return False
        if hyphen and previous_hyphen:
            return False
        previous_hyphen = hyphen
    return not previous_hyphen


def _utf8_boundaries(text: str) -> set[int]:
    boundaries = {0}
    offset = 0
    for character in text:
        offset += len(character.encode("utf-8"))
        boundaries.add(offset)
    return boundaries


def parse_record(record: Mapping[str, object], source: str = "record") -> TrainingExample:
    """Parse and fully validate one schema-v1 mapping."""

    if _integer(record.get("schema_version"), f"{source}.schema_version") != SCHEMA_VERSION:
        raise DatasetError(f"{source}.schema_version is not supported")
    identifier = record.get("id")
    text = record.get("text")
    language = record.get("language")
    raw_units = record.get("units")
    if not isinstance(identifier, str) or not identifier:
        raise DatasetError(f"{source}.id must be a nonempty string")
    if not isinstance(text, str):
        raise DatasetError(f"{source}.text must be a string")
    if not isinstance(language, str) or not _language_is_valid(language):
        raise DatasetError(f"{source}.language must be a BCP-47-compatible ASCII tag")
    if not isinstance(raw_units, list):
        raise DatasetError(f"{source}.units must be an array")

    text_size = len(text.encode("utf-8"))
    utf8_boundaries = _utf8_boundaries(text)
    units: list[UnitTarget] = []
    previous_end = 0
    for index, raw_unit in enumerate(raw_units):
        unit_source = f"{source}.units[{index}]"
        if not isinstance(raw_unit, Mapping):
            raise DatasetError(f"{unit_source} must be an object")
        begin = _integer(raw_unit.get("begin"), f"{unit_source}.begin")
        end = _integer(raw_unit.get("end"), f"{unit_source}.end")
        target = _integer(
            raw_unit.get("target_prefix_graphemes"),
            f"{unit_source}.target_prefix_graphemes",
        )
        raw_boundaries = raw_unit.get("grapheme_boundaries")
        if not isinstance(raw_boundaries, list):
            raise DatasetError(f"{unit_source}.grapheme_boundaries must be an array")
        boundaries = tuple(
            _integer(value, f"{unit_source}.grapheme_boundaries[{boundary_index}]")
            for boundary_index, value in enumerate(raw_boundaries)
        )
        if begin < previous_end or begin < 0 or end <= begin or end > text_size:
            raise DatasetError(f"{unit_source} must be ordered, nonoverlapping, and nonempty")
        if begin not in utf8_boundaries or end not in utf8_boundaries:
            raise DatasetError(f"{unit_source} splits a UTF-8 sequence")
        if len(boundaries) < 2 or boundaries[0] != begin or boundaries[-1] != end:
            raise DatasetError(f"{unit_source}.grapheme_boundaries must cover the unit")
        if any(left >= right for left, right in zip(boundaries, boundaries[1:])):
            raise DatasetError(f"{unit_source}.grapheme_boundaries must be strictly increasing")
        if any(boundary not in utf8_boundaries for boundary in boundaries):
            raise DatasetError(f"{unit_source}.grapheme_boundaries split a UTF-8 sequence")
        grapheme_count = len(boundaries) - 1
        if target < 0 or target > grapheme_count:
            raise DatasetError(f"{unit_source}.target_prefix_graphemes is outside the unit")
        units.append(UnitTarget(begin, end, boundaries, target))
        previous_end = end
    if not units:
        raise DatasetError(f"{source}.units must not be empty")
    return TrainingExample(identifier, text, language, tuple(units))


class JsonlDataset(Iterable[TrainingExample]):
    """A repeatable JSON Lines dataset with validation on every iteration."""

    def __init__(self, path: str | Path):
        self.path = Path(path)

    def __iter__(self) -> Iterator[TrainingExample]:
        identifiers: set[str] = set()
        try:
            stream = self.path.open("r", encoding="utf-8")
        except OSError as error:
            raise DatasetError(f"could not open dataset {self.path}: {error}") from error
        with stream:
            for line_number, line in enumerate(stream, 1):
                if not line.strip():
                    continue
                source = f"{self.path}:{line_number}"
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as error:
                    raise DatasetError(f"{source} is not valid JSON: {error.msg}") from error
                if not isinstance(value, Mapping):
                    raise DatasetError(f"{source} must contain a JSON object")
                example = parse_record(value, source)
                if example.identifier in identifiers:
                    raise DatasetError(f"{source} repeats id {example.identifier!r}")
                identifiers.add(example.identifier)
                yield example
