"""Deterministic ridge regression for the first learned reading model."""

from __future__ import annotations

import math
import struct
from collections.abc import Iterable, Sequence
from dataclasses import asdict, dataclass

from .salience_dataset import KNOWN_FEATURE_IDS, SalienceExample, SalienceUnit


@dataclass(frozen=True, slots=True)
class LinearSalienceMetrics:
    unit_count: int
    mean_absolute_error: float
    root_mean_square_error: float
    r_squared: float | None

    def as_dict(self) -> dict[str, int | float | None]:
        return asdict(self)


@dataclass(frozen=True, slots=True)
class FittedLinearSalience:
    bias: float
    weights: tuple[tuple[int, float], ...]
    metrics: LinearSalienceMetrics
    ridge: float

    def predict(self, unit: SalienceUnit) -> float:
        value = self.bias
        for feature_id, weight in self.weights:
            value += weight * unit.feature_value(feature_id)
        return min(max(value, 0.0), 1.0)


def _binary32(value: float) -> float:
    try:
        result = struct.unpack("<f", struct.pack("<f", value))[0]
    except (OverflowError, struct.error) as error:
        raise ValueError("learned parameter does not fit IEEE-754 binary32") from error
    if not math.isfinite(result):
        raise ValueError("learned parameter is not finite")
    return result


def _parameter(value: float) -> float:
    return _binary32(0.0 if abs(value) < 1e-12 else value)


def _solve(matrix: list[list[float]], target: list[float]) -> list[float]:
    size = len(target)
    augmented = [row[:] + [target[index]] for index, row in enumerate(matrix)]
    for column in range(size):
        pivot = max(range(column, size), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) < 1e-12:
            raise ValueError("linear system is singular; select fewer features or use ridge")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        divisor = augmented[column][column]
        augmented[column] = [value / divisor for value in augmented[column]]
        for row in range(size):
            if row == column:
                continue
            factor = augmented[row][column]
            if factor == 0.0:
                continue
            augmented[row] = [
                value - factor * pivot_value
                for value, pivot_value in zip(augmented[row], augmented[column])
            ]
    return [augmented[index][-1] for index in range(size)]


def linear_salience_metrics(
    units: Sequence[SalienceUnit], predictions: Sequence[float]
) -> LinearSalienceMetrics:
    """Score already-produced predictions against labeled runtime units."""

    if not units or len(units) != len(predictions):
        raise ValueError("metrics require one prediction for every nonempty unit")
    if any(not math.isfinite(prediction) for prediction in predictions):
        raise ValueError("predictions must be finite")
    targets = tuple(unit.target_salience for unit in units)
    errors = tuple(predicted - target for predicted, target in zip(predictions, targets))
    target_mean = sum(targets) / len(targets)
    residual = sum(error * error for error in errors)
    total = sum((target - target_mean) ** 2 for target in targets)
    return LinearSalienceMetrics(
        unit_count=len(units),
        mean_absolute_error=sum(abs(error) for error in errors) / len(errors),
        root_mean_square_error=math.sqrt(residual / len(errors)),
        r_squared=1.0 - residual / total if total > 0.0 else None,
    )


def evaluate_linear_salience(
    examples: Iterable[SalienceExample], model: FittedLinearSalience
) -> LinearSalienceMetrics:
    """Evaluate a fitted model on examples that were not necessarily used to fit it."""

    units = tuple(unit for example in examples for unit in example.units)
    return linear_salience_metrics(units, tuple(model.predict(unit) for unit in units))


def fit_linear_salience(
    examples: Iterable[SalienceExample],
    feature_ids: Sequence[int] | None = None,
    ridge: float = 1e-6,
) -> FittedLinearSalience:
    """Fit a small linear predictor and quantize it to runtime binary32 values."""

    values = tuple(examples)
    units = tuple(unit for example in values for unit in example.units)
    if not units:
        raise ValueError("linear salience training requires at least one unit")
    if not math.isfinite(ridge) or ridge < 0.0:
        raise ValueError("ridge must be finite and nonnegative")
    selected = (
        tuple(sorted({feature.feature_id for unit in units for feature in unit.features}))
        if feature_ids is None
        else tuple(feature_ids)
    )
    if (
        not selected
        or len(selected) > 256
        or any(type(feature_id) is not int for feature_id in selected)
        or len(set(selected)) != len(selected)
    ):
        raise ValueError("feature selection must contain between 1 and 256 unique IDs")
    if any(feature_id not in KNOWN_FEATURE_IDS for feature_id in selected):
        raise ValueError("feature selection contains an unsupported runtime feature")

    width = len(selected) + 1
    normal = [[0.0 for _ in range(width)] for _ in range(width)]
    target = [0.0 for _ in range(width)]
    for unit in units:
        row = [1.0] + [unit.feature_value(feature_id) for feature_id in selected]
        for left in range(width):
            target[left] += row[left] * unit.target_salience
            for right in range(width):
                normal[left][right] += row[left] * row[right]
    for index in range(1, width):
        normal[index][index] += ridge
    solution = _solve(normal, target)
    provisional = FittedLinearSalience(
        bias=_parameter(solution[0]),
        weights=tuple(
            (feature_id, _parameter(weight))
            for feature_id, weight in zip(selected, solution[1:])
        ),
        metrics=LinearSalienceMetrics(0, 0.0, 0.0, None),
        ridge=ridge,
    )
    return FittedLinearSalience(
        provisional.bias,
        provisional.weights,
        linear_salience_metrics(units, tuple(provisional.predict(unit) for unit in units)),
        ridge,
    )
