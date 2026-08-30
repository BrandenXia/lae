"""Shared validation helpers for versioned evaluation records."""

from __future__ import annotations

import json
import math
from collections.abc import Iterator, Mapping
from pathlib import Path


class EvaluationDataError(ValueError):
    """Raised when an evaluation input violates its schema."""


def integer(value: object, field: str, *, minimum: int | None = None) -> int:
    if type(value) is not int:
        raise EvaluationDataError(f"{field} must be an integer")
    if minimum is not None and value < minimum:
        raise EvaluationDataError(f"{field} must be at least {minimum}")
    return value


def number(value: object, field: str, *, minimum: float | None = None) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise EvaluationDataError(f"{field} must be a number")
    result = float(value)
    if not math.isfinite(result):
        raise EvaluationDataError(f"{field} must be finite")
    if minimum is not None and result < minimum:
        raise EvaluationDataError(f"{field} must be at least {minimum}")
    return result


def score(value: object, field: str) -> float:
    result = number(value, field)
    if result > 1.0:
        raise EvaluationDataError(f"{field} must be in [0, 1]")
    if result < 0.0:
        raise EvaluationDataError(f"{field} must be in [0, 1]")
    return result


def identifier(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise EvaluationDataError(f"{field} must be a nonempty string")
    return value


def records(path: str | Path) -> Iterator[tuple[Mapping[str, object], str]]:
    source_path = Path(path)
    try:
        stream = source_path.open("r", encoding="utf-8")
    except OSError as error:
        raise EvaluationDataError(
            f"could not open evaluation data {source_path}: {error}"
        ) from error
    with stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            source = f"{source_path}:{line_number}"
            try:
                value = json.loads(line)
            except json.JSONDecodeError as error:
                raise EvaluationDataError(f"{source} is not valid JSON: {error.msg}") from error
            if not isinstance(value, Mapping):
                raise EvaluationDataError(f"{source} must contain a JSON object")
            yield value, source
