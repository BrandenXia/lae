"""Offline emphasis-plan metrics that are independent of model strategy."""

from __future__ import annotations

from collections import defaultdict
from collections.abc import Iterable, Iterator, Mapping
from dataclasses import asdict, dataclass
from pathlib import Path

from .evaluation_records import EvaluationDataError, identifier, integer, records


SCHEMA_VERSION = 1


@dataclass(frozen=True, slots=True)
class GraphemeRange:
    begin: int
    end: int


@dataclass(frozen=True, slots=True)
class PlanRecord:
    example_id: str
    variant: str
    grapheme_count: int
    line_count: int
    emphasized_ranges: tuple[GraphemeRange, ...]

    @property
    def emphasized_graphemes(self) -> int:
        return sum(item.end - item.begin for item in self.emphasized_ranges)

    @property
    def transition_count(self) -> int:
        return sum(
            int(item.begin > 0) + int(item.end < self.grapheme_count)
            for item in self.emphasized_ranges
        )


@dataclass(frozen=True, slots=True)
class PlanMetrics:
    variant: str
    example_count: int
    grapheme_count: int
    line_count: int
    emphasized_graphemes: int
    emphasis_span_count: int
    emphasis_transition_count: int
    emphasis_density: float
    text_density_graphemes_per_line: float
    spans_per_100_graphemes: float
    transitions_per_100_boundaries: float

    def as_dict(self) -> dict[str, str | int | float]:
        return asdict(self)


@dataclass(frozen=True, slots=True)
class PlanComparison:
    baseline: str
    candidate: str
    paired_examples: int
    mean_emphasis_density_delta: float
    mean_text_density_graphemes_per_line_delta: float
    mean_spans_per_100_graphemes_delta: float
    mean_transitions_per_100_boundaries_delta: float

    def as_dict(self) -> dict[str, str | int | float]:
        return asdict(self)


def parse_plan_record(record: Mapping[str, object], source: str = "record") -> PlanRecord:
    if integer(record.get("schema_version"), f"{source}.schema_version") != SCHEMA_VERSION:
        raise EvaluationDataError(f"{source}.schema_version is not supported")
    example_id = identifier(record.get("example_id"), f"{source}.example_id")
    variant = identifier(record.get("variant"), f"{source}.variant")
    grapheme_count = integer(record.get("grapheme_count"), f"{source}.grapheme_count", minimum=1)
    line_count = integer(record.get("line_count"), f"{source}.line_count", minimum=1)
    raw_ranges = record.get("emphasized_ranges")
    if not isinstance(raw_ranges, list):
        raise EvaluationDataError(f"{source}.emphasized_ranges must be an array")
    emphasized_ranges: list[GraphemeRange] = []
    previous_end = 0
    for index, raw_range in enumerate(raw_ranges):
        range_source = f"{source}.emphasized_ranges[{index}]"
        if not isinstance(raw_range, Mapping):
            raise EvaluationDataError(f"{range_source} must be an object")
        begin = integer(raw_range.get("begin"), f"{range_source}.begin", minimum=0)
        end = integer(raw_range.get("end"), f"{range_source}.end", minimum=0)
        if (emphasized_ranges and begin <= previous_end) or end <= begin or end > grapheme_count:
            raise EvaluationDataError(
                f"{range_source} must be ordered, separated, nonempty, and in bounds"
            )
        emphasized_ranges.append(GraphemeRange(begin, end))
        previous_end = end
    return PlanRecord(example_id, variant, grapheme_count, line_count, tuple(emphasized_ranges))


class PlanJsonlDataset(Iterable[PlanRecord]):
    def __init__(self, path: str | Path):
        self.path = Path(path)

    def __iter__(self) -> Iterator[PlanRecord]:
        seen: set[tuple[str, str]] = set()
        for value, source in records(self.path):
            record = parse_plan_record(value, source)
            key = (record.example_id, record.variant)
            if key in seen:
                raise EvaluationDataError(
                    f"{source} repeats example {record.example_id!r} for variant {record.variant!r}"
                )
            seen.add(key)
            yield record


def _metrics(variant: str, items: Iterable[PlanRecord]) -> PlanMetrics:
    values = tuple(items)
    graphemes = sum(item.grapheme_count for item in values)
    lines = sum(item.line_count for item in values)
    emphasized = sum(item.emphasized_graphemes for item in values)
    spans = sum(len(item.emphasized_ranges) for item in values)
    transitions = sum(item.transition_count for item in values)
    boundaries = sum(max(item.grapheme_count - 1, 0) for item in values)
    return PlanMetrics(
        variant=variant,
        example_count=len(values),
        grapheme_count=graphemes,
        line_count=lines,
        emphasized_graphemes=emphasized,
        emphasis_span_count=spans,
        emphasis_transition_count=transitions,
        emphasis_density=emphasized / graphemes,
        text_density_graphemes_per_line=graphemes / lines,
        spans_per_100_graphemes=100.0 * spans / graphemes,
        transitions_per_100_boundaries=100.0 * transitions / boundaries if boundaries else 0.0,
    )


def aggregate_plans(items: Iterable[PlanRecord]) -> tuple[PlanMetrics, ...]:
    grouped: dict[str, list[PlanRecord]] = defaultdict(list)
    for item in items:
        grouped[item.variant].append(item)
    if not grouped:
        raise ValueError("plan evaluation requires at least one record")
    return tuple(_metrics(variant, grouped[variant]) for variant in sorted(grouped, key=str.lower))


def _record_rates(item: PlanRecord) -> tuple[float, float, float, float]:
    boundaries = max(item.grapheme_count - 1, 0)
    return (
        item.emphasized_graphemes / item.grapheme_count,
        item.grapheme_count / item.line_count,
        100.0 * len(item.emphasized_ranges) / item.grapheme_count,
        100.0 * item.transition_count / boundaries if boundaries else 0.0,
    )


def compare_plans(items: Iterable[PlanRecord], baseline: str) -> tuple[PlanComparison, ...]:
    values = tuple(items)
    baseline_by_example = {item.example_id: item for item in values if item.variant == baseline}
    if not baseline_by_example:
        raise ValueError(f"baseline variant {baseline!r} is absent")
    variants = sorted({item.variant for item in values if item.variant != baseline}, key=str.lower)
    comparisons: list[PlanComparison] = []
    for variant in variants:
        candidate_by_example = {item.example_id: item for item in values if item.variant == variant}
        keys = sorted(baseline_by_example.keys() & candidate_by_example.keys())
        if not keys:
            continue
        deltas: list[tuple[float, float, float, float]] = []
        for key in keys:
            baseline_item = baseline_by_example[key]
            candidate_item = candidate_by_example[key]
            if baseline_item.grapheme_count != candidate_item.grapheme_count:
                raise ValueError(f"paired plan metadata differs for example {key!r}")
            baseline_rates = _record_rates(baseline_item)
            candidate_rates = _record_rates(candidate_item)
            deltas.append(
                tuple(right - left for left, right in zip(baseline_rates, candidate_rates))
            )
        comparisons.append(
            PlanComparison(
                baseline=baseline,
                candidate=variant,
                paired_examples=len(deltas),
                mean_emphasis_density_delta=sum(item[0] for item in deltas) / len(deltas),
                mean_text_density_graphemes_per_line_delta=sum(item[1] for item in deltas)
                / len(deltas),
                mean_spans_per_100_graphemes_delta=sum(item[2] for item in deltas) / len(deltas),
                mean_transitions_per_100_boundaries_delta=sum(item[3] for item in deltas)
                / len(deltas),
            )
        )
    return tuple(comparisons)
