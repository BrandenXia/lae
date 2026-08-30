"""Deterministic optimization for the baseline runtime prefix model."""

from __future__ import annotations

from collections.abc import Iterable, Sequence
from dataclasses import dataclass

from .evaluation import PrefixCandidate, PrefixMetrics, evaluate_prefix
from .features import UnitFeatures


DEFAULT_PROPORTIONS = tuple(index / 20 for index in range(21))


@dataclass(frozen=True, slots=True)
class FittedPrefix:
    candidate: PrefixCandidate
    metrics: PrefixMetrics
    candidates_evaluated: int


def _score(candidate: PrefixCandidate, metrics: PrefixMetrics) -> tuple[float, ...]:
    return (
        metrics.mean_absolute_error,
        metrics.root_mean_square_error,
        abs(metrics.predicted_density - metrics.target_density),
        0.0 if candidate.strategy == "fixed" else 1.0,
        float(candidate.fixed_graphemes)
        if candidate.strategy == "fixed"
        else candidate.proportion,
    )


def fit_prefix(
    features: Iterable[UnitFeatures],
    strategy: str = "auto",
    proportions: Sequence[float] = DEFAULT_PROPORTIONS,
) -> FittedPrefix:
    """Grid-search the small parameter space exported by artifact format v1."""

    rows = tuple(features)
    if not rows:
        raise ValueError("prefix optimization requires at least one unit")
    if strategy not in {"auto", "fixed", "proportional"}:
        raise ValueError("strategy must be auto, fixed, or proportional")

    candidates: list[PrefixCandidate] = []
    if strategy in {"auto", "fixed"}:
        maximum = max(row.graphemes for row in rows)
        candidates.extend(
            PrefixCandidate("fixed", fixed_graphemes=count) for count in range(maximum + 1)
        )
    if strategy in {"auto", "proportional"}:
        candidates.extend(
            PrefixCandidate("proportional", proportion=value) for value in proportions
        )
    if not candidates:
        raise ValueError("optimization has no candidates")

    evaluated = [(candidate, evaluate_prefix(rows, candidate)) for candidate in candidates]
    candidate, metrics = min(evaluated, key=lambda item: _score(*item))
    return FittedPrefix(candidate, metrics, len(evaluated))
