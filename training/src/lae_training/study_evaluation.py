"""Human-study aggregation and paired A/B comparisons."""

from __future__ import annotations

from collections import defaultdict
from collections.abc import Iterable, Iterator, Mapping
from dataclasses import asdict, dataclass
from pathlib import Path

from .evaluation_records import (
    EvaluationDataError,
    identifier,
    integer,
    number,
    records,
    score,
)


SCHEMA_VERSION = 1


@dataclass(frozen=True, slots=True)
class StudyObservation:
    participant_id: str
    example_id: str
    variant: str
    duration_ms: float
    content_unit_count: int
    grapheme_count: int
    line_count: int
    emphasized_graphemes: int
    comprehension_score: float
    fixation_duration_ms: float | None
    fixation_count: int | None
    regression_count: int | None
    preference_score: float | None
    distraction_score: float | None

    @property
    def reading_speed(self) -> float:
        return 60_000.0 * self.content_unit_count / self.duration_ms

    @property
    def emphasis_density(self) -> float:
        return self.emphasized_graphemes / self.grapheme_count

    @property
    def text_density(self) -> float:
        return self.grapheme_count / self.line_count


@dataclass(frozen=True, slots=True)
class StudyMetrics:
    variant: str
    observation_count: int
    participant_count: int
    fixation_duration_observation_count: int
    fixation_count_observation_count: int
    regression_observation_count: int
    preference_observation_count: int
    distraction_observation_count: int
    mean_reading_speed_units_per_minute: float
    mean_comprehension_score: float
    mean_fixation_duration_ms: float | None
    fixation_count_per_100_content_units: float | None
    regressions_per_100_content_units: float | None
    mean_preference_score: float | None
    mean_distraction_score: float | None
    emphasis_density: float
    text_density_graphemes_per_line: float

    def as_dict(self) -> dict[str, str | int | float | None]:
        return asdict(self)


@dataclass(frozen=True, slots=True)
class StudyComparison:
    baseline: str
    candidate: str
    paired_observations: int
    fixation_duration_pairs: int
    fixation_count_pairs: int
    regression_pairs: int
    preference_pairs: int
    distraction_pairs: int
    mean_reading_speed_delta: float
    mean_comprehension_delta: float
    mean_fixation_duration_delta_ms: float | None
    mean_fixation_count_per_100_units_delta: float | None
    mean_regressions_per_100_units_delta: float | None
    mean_preference_delta: float | None
    mean_distraction_delta: float | None
    mean_emphasis_density_delta: float
    mean_text_density_graphemes_per_line_delta: float

    def as_dict(self) -> dict[str, str | int | float | None]:
        return asdict(self)


def _optional_number(
    record: Mapping[str, object], field: str, source: str, *, minimum: float | None = None
) -> float | None:
    value = record.get(field)
    return None if value is None else number(value, f"{source}.{field}", minimum=minimum)


def _optional_integer(
    record: Mapping[str, object], field: str, source: str, *, minimum: int = 0
) -> int | None:
    value = record.get(field)
    return None if value is None else integer(value, f"{source}.{field}", minimum=minimum)


def _optional_score(record: Mapping[str, object], field: str, source: str) -> float | None:
    value = record.get(field)
    return None if value is None else score(value, f"{source}.{field}")


def parse_study_observation(
    record: Mapping[str, object], source: str = "record"
) -> StudyObservation:
    if integer(record.get("schema_version"), f"{source}.schema_version") != SCHEMA_VERSION:
        raise EvaluationDataError(f"{source}.schema_version is not supported")
    participant_id = identifier(record.get("participant_id"), f"{source}.participant_id")
    example_id = identifier(record.get("example_id"), f"{source}.example_id")
    variant = identifier(record.get("variant"), f"{source}.variant")
    duration_ms = number(record.get("duration_ms"), f"{source}.duration_ms", minimum=0.0)
    if duration_ms == 0.0:
        raise EvaluationDataError(f"{source}.duration_ms must be positive")
    content_units = integer(
        record.get("content_unit_count"), f"{source}.content_unit_count", minimum=1
    )
    graphemes = integer(record.get("grapheme_count"), f"{source}.grapheme_count", minimum=1)
    lines = integer(record.get("line_count"), f"{source}.line_count", minimum=1)
    emphasized = integer(
        record.get("emphasized_graphemes"), f"{source}.emphasized_graphemes", minimum=0
    )
    if emphasized > graphemes:
        raise EvaluationDataError(f"{source}.emphasized_graphemes exceeds grapheme_count")
    comprehension = score(record.get("comprehension_score"), f"{source}.comprehension_score")
    fixation_duration = _optional_number(
        record, "fixation_duration_ms", source, minimum=0.0
    )
    fixation_count = _optional_integer(record, "fixation_count", source)
    if (fixation_duration is None) != (fixation_count is None):
        raise EvaluationDataError(
            f"{source}.fixation_duration_ms and fixation_count must appear together"
        )
    if fixation_duration is not None and fixation_count == 0 and fixation_duration != 0.0:
        raise EvaluationDataError(f"{source} has fixation duration without any fixations")
    return StudyObservation(
        participant_id=participant_id,
        example_id=example_id,
        variant=variant,
        duration_ms=duration_ms,
        content_unit_count=content_units,
        grapheme_count=graphemes,
        line_count=lines,
        emphasized_graphemes=emphasized,
        comprehension_score=comprehension,
        fixation_duration_ms=fixation_duration,
        fixation_count=fixation_count,
        regression_count=_optional_integer(record, "regression_count", source),
        preference_score=_optional_score(record, "preference_score", source),
        distraction_score=_optional_score(record, "distraction_score", source),
    )


class StudyJsonlDataset(Iterable[StudyObservation]):
    def __init__(self, path: str | Path):
        self.path = Path(path)

    def __iter__(self) -> Iterator[StudyObservation]:
        seen: set[tuple[str, str, str]] = set()
        for value, source in records(self.path):
            observation = parse_study_observation(value, source)
            key = (observation.participant_id, observation.example_id, observation.variant)
            if key in seen:
                raise EvaluationDataError(f"{source} repeats participant/example/variant")
            seen.add(key)
            yield observation


def _mean(values: Iterable[float]) -> float | None:
    items = tuple(values)
    return sum(items) / len(items) if items else None


def _metrics(variant: str, items: Iterable[StudyObservation]) -> StudyMetrics:
    values = tuple(items)
    fixation_values = tuple(item for item in values if item.fixation_count is not None)
    regression_values = tuple(item for item in values if item.regression_count is not None)
    fixation_count = sum(item.fixation_count or 0 for item in fixation_values)
    fixation_duration = sum(item.fixation_duration_ms or 0.0 for item in fixation_values)
    fixation_units = sum(item.content_unit_count for item in fixation_values)
    regression_units = sum(item.content_unit_count for item in regression_values)
    return StudyMetrics(
        variant=variant,
        observation_count=len(values),
        participant_count=len({item.participant_id for item in values}),
        fixation_duration_observation_count=sum(
            (item.fixation_count or 0) > 0 for item in fixation_values
        ),
        fixation_count_observation_count=len(fixation_values),
        regression_observation_count=len(regression_values),
        preference_observation_count=sum(item.preference_score is not None for item in values),
        distraction_observation_count=sum(item.distraction_score is not None for item in values),
        mean_reading_speed_units_per_minute=sum(item.reading_speed for item in values)
        / len(values),
        mean_comprehension_score=sum(item.comprehension_score for item in values) / len(values),
        mean_fixation_duration_ms=fixation_duration / fixation_count if fixation_count else None,
        fixation_count_per_100_content_units=(
            100.0 * fixation_count / fixation_units if fixation_values else None
        ),
        regressions_per_100_content_units=(
            100.0 * sum(item.regression_count or 0 for item in regression_values)
            / regression_units
            if regression_values
            else None
        ),
        mean_preference_score=_mean(
            item.preference_score for item in values if item.preference_score is not None
        ),
        mean_distraction_score=_mean(
            item.distraction_score for item in values if item.distraction_score is not None
        ),
        emphasis_density=sum(item.emphasized_graphemes for item in values)
        / sum(item.grapheme_count for item in values),
        text_density_graphemes_per_line=sum(item.grapheme_count for item in values)
        / sum(item.line_count for item in values),
    )


def aggregate_study(items: Iterable[StudyObservation]) -> tuple[StudyMetrics, ...]:
    grouped: dict[str, list[StudyObservation]] = defaultdict(list)
    for item in items:
        grouped[item.variant].append(item)
    if not grouped:
        raise ValueError("study evaluation requires at least one observation")
    return tuple(_metrics(variant, grouped[variant]) for variant in sorted(grouped, key=str.lower))


def _optional_delta(left: float | None, right: float | None) -> float | None:
    return None if left is None or right is None else right - left


def _fixation_mean(item: StudyObservation) -> float | None:
    if not item.fixation_count:
        return None
    return (item.fixation_duration_ms or 0.0) / item.fixation_count


def _fixation_rate(item: StudyObservation) -> float | None:
    if item.fixation_count is None:
        return None
    return 100.0 * item.fixation_count / item.content_unit_count


def _regression_rate(item: StudyObservation) -> float | None:
    if item.regression_count is None:
        return None
    return 100.0 * item.regression_count / item.content_unit_count


def _mean_present(values: Iterable[float | None]) -> float | None:
    present = tuple(value for value in values if value is not None)
    return sum(present) / len(present) if present else None


def compare_study(
    items: Iterable[StudyObservation], baseline: str
) -> tuple[StudyComparison, ...]:
    values = tuple(items)
    baseline_items = {
        (item.participant_id, item.example_id): item for item in values if item.variant == baseline
    }
    if not baseline_items:
        raise ValueError(f"baseline variant {baseline!r} is absent")
    variants = sorted({item.variant for item in values if item.variant != baseline}, key=str.lower)
    results: list[StudyComparison] = []
    for variant in variants:
        candidate_items = {
            (item.participant_id, item.example_id): item
            for item in values
            if item.variant == variant
        }
        keys = sorted(baseline_items.keys() & candidate_items.keys())
        if not keys:
            continue
        pairs: list[tuple[StudyObservation, StudyObservation]] = []
        for key in keys:
            left = baseline_items[key]
            right = candidate_items[key]
            if (
                left.content_unit_count != right.content_unit_count
                or left.grapheme_count != right.grapheme_count
            ):
                raise ValueError(f"paired study metadata differs for {key!r}")
            pairs.append((left, right))
        fixation_deltas = tuple(
            value
            for left, right in pairs
            if (value := _optional_delta(_fixation_mean(left), _fixation_mean(right)))
            is not None
        )
        fixation_count_deltas = tuple(
            value
            for left, right in pairs
            if (value := _optional_delta(_fixation_rate(left), _fixation_rate(right)))
            is not None
        )
        regression_deltas = tuple(
            value
            for left, right in pairs
            if (value := _optional_delta(_regression_rate(left), _regression_rate(right)))
            is not None
        )
        preference_deltas = tuple(
            value
            for left, right in pairs
            if (value := _optional_delta(left.preference_score, right.preference_score))
            is not None
        )
        distraction_deltas = tuple(
            value
            for left, right in pairs
            if (value := _optional_delta(left.distraction_score, right.distraction_score))
            is not None
        )
        results.append(
            StudyComparison(
                baseline=baseline,
                candidate=variant,
                paired_observations=len(pairs),
                fixation_duration_pairs=len(fixation_deltas),
                fixation_count_pairs=len(fixation_count_deltas),
                regression_pairs=len(regression_deltas),
                preference_pairs=len(preference_deltas),
                distraction_pairs=len(distraction_deltas),
                mean_reading_speed_delta=sum(
                    right.reading_speed - left.reading_speed for left, right in pairs
                )
                / len(pairs),
                mean_comprehension_delta=sum(
                    right.comprehension_score - left.comprehension_score for left, right in pairs
                )
                / len(pairs),
                mean_fixation_duration_delta_ms=_mean_present(fixation_deltas),
                mean_fixation_count_per_100_units_delta=_mean_present(fixation_count_deltas),
                mean_regressions_per_100_units_delta=_mean_present(regression_deltas),
                mean_preference_delta=_mean_present(preference_deltas),
                mean_distraction_delta=_mean_present(distraction_deltas),
                mean_emphasis_density_delta=sum(
                    right.emphasis_density - left.emphasis_density for left, right in pairs
                )
                / len(pairs),
                mean_text_density_graphemes_per_line_delta=sum(
                    right.text_density - left.text_density for left, right in pairs
                )
                / len(pairs),
            )
        )
    return tuple(results)
