"""Deterministic feature extraction from validated training examples."""

from __future__ import annotations

import unicodedata
from collections.abc import Iterable, Iterator
from dataclasses import dataclass

from .dataset import TrainingExample


@dataclass(frozen=True, slots=True)
class UnitFeatures:
    example_id: str
    language: str
    begin: int
    end: int
    utf8_bytes: int
    code_points: int
    graphemes: int
    combining_marks: int
    alphabetic_fraction: float
    numeric_fraction: float
    punctuation_fraction: float
    target_prefix_graphemes: int
    target_density: float


def extract_features(examples: Iterable[TrainingExample]) -> Iterator[UnitFeatures]:
    """Yield language-neutral scalar features for every annotated unit."""

    for example in examples:
        encoded = example.text.encode("utf-8")
        for unit in example.units:
            unit_text = encoded[unit.begin : unit.end].decode("utf-8")
            code_points = len(unit_text)
            denominator = max(code_points, 1)
            combining_marks = sum(unicodedata.combining(character) != 0 for character in unit_text)
            alphabetic = sum(character.isalpha() for character in unit_text)
            numeric = sum(character.isnumeric() for character in unit_text)
            punctuation = sum(
                unicodedata.category(character).startswith("P") for character in unit_text
            )
            yield UnitFeatures(
                example_id=example.identifier,
                language=example.language,
                begin=unit.begin,
                end=unit.end,
                utf8_bytes=unit.end - unit.begin,
                code_points=code_points,
                graphemes=unit.grapheme_count,
                combining_marks=combining_marks,
                alphabetic_fraction=alphabetic / denominator,
                numeric_fraction=numeric / denominator,
                punctuation_fraction=punctuation / denominator,
                target_prefix_graphemes=unit.target_prefix_graphemes,
                target_density=unit.target_prefix_graphemes / unit.grapheme_count,
            )
