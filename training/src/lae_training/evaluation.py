"""Small offline metrics for deterministic prefix-model experiments."""

from __future__ import annotations

import math
import struct
from collections.abc import Iterable
from dataclasses import asdict, dataclass

from .features import UnitFeatures


@dataclass(frozen=True, slots=True)
class PrefixCandidate:
    strategy: str
    fixed_graphemes: int = 1
    proportion: float = 0.5

    def __post_init__(self) -> None:
        if self.strategy not in {"fixed", "proportional"}:
            raise ValueError("prefix strategy must be fixed or proportional")
        if self.fixed_graphemes < 0 or self.fixed_graphemes > 0xFFFFFFFF:
            raise ValueError("fixed grapheme count must fit uint32")
        if not math.isfinite(self.proportion) or not 0.0 <= self.proportion <= 1.0:
            raise ValueError("prefix proportion must be finite and in [0, 1]")
        runtime_proportion = struct.unpack("<f", struct.pack("<f", self.proportion))[0]
        object.__setattr__(self, "proportion", runtime_proportion)

    def predict(self, grapheme_count: int) -> int:
        if grapheme_count <= 0:
            return 0
        if self.strategy == "fixed":
            return min(self.fixed_graphemes, grapheme_count)
        if self.proportion == 0.0:
            return 0
        return min(max(math.ceil(grapheme_count * self.proportion), 1), grapheme_count)


@dataclass(frozen=True, slots=True)
class PrefixMetrics:
    unit_count: int
    mean_absolute_error: float
    root_mean_square_error: float
    exact_match_rate: float
    target_density: float
    predicted_density: float

    def as_dict(self) -> dict[str, int | float]:
        return asdict(self)


def evaluate_prefix(
    features: Iterable[UnitFeatures], candidate: PrefixCandidate
) -> PrefixMetrics:
    """Compare a prefix candidate with annotated target grapheme counts."""

    rows = tuple(features)
    if not rows:
        raise ValueError("evaluation requires at least one unit")
    absolute_error = 0
    squared_error = 0
    exact = 0
    target_total = 0
    predicted_total = 0
    grapheme_total = 0
    for row in rows:
        predicted = candidate.predict(row.graphemes)
        difference = predicted - row.target_prefix_graphemes
        absolute_error += abs(difference)
        squared_error += difference * difference
        exact += difference == 0
        target_total += row.target_prefix_graphemes
        predicted_total += predicted
        grapheme_total += row.graphemes
    unit_count = len(rows)
    return PrefixMetrics(
        unit_count=unit_count,
        mean_absolute_error=absolute_error / unit_count,
        root_mean_square_error=math.sqrt(squared_error / unit_count),
        exact_match_rate=exact / unit_count,
        target_density=target_total / grapheme_total,
        predicted_density=predicted_total / grapheme_total,
    )
