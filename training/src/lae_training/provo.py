"""Reproducible real-data training on the Provo eye-tracking corpus."""

from __future__ import annotations

import csv
import hashlib
import json
import re
import subprocess
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

from .artifacts import ABI_VERSION, FORMAT_VERSION, build_linear_salience_artifact
from .dataset import DatasetError
from .linear_salience import (
    FittedLinearSalience,
    LinearSalienceMetrics,
    fit_linear_salience,
    linear_salience_metrics,
)
from .salience_dataset import SalienceExample, SalienceFeature, SalienceUnit


PROVO_PROJECT_URL = "https://osf.io/sjefs/"
PROVO_DATA_URL = "https://osf.io/download/a32be/"
PROVO_DOI = "10.3758/s13428-017-0908-4"
PROVO_LICENSE = "CC BY 4.0"
PROVO_LICENSE_URL = "https://creativecommons.org/licenses/by/4.0/"
PROVO_SHA256 = "38aedcb29bc9171009916eb2bcc2375729f104a2a1005c64a563da94b611b9e7"

FEATURE_GRAPHEME_COUNT = 0x00000002
FEATURE_CONTENT_UNIT = 0x00030001
FEATURE_FUNCTION_UNIT = 0x00030002
FEATURE_NAMES = {
    FEATURE_GRAPHEME_COUNT: "grapheme_count",
    FEATURE_FUNCTION_UNIT: "function_unit",
}
_REQUIRED_COLUMNS = frozenset(
    {
        "Participant_ID",
        "Word_Unique_ID",
        "Text_ID",
        "Word_Number",
        "Word_Cleaned",
        "IA_ID",
        "IA_SKIP",
    }
)
_SINGLE_RUNTIME_UNIT = re.compile(r"[A-Za-z]+(?:'[A-Za-z]+)?").fullmatch
_RIDGES = (0.0, 0.01, 0.1, 1.0, 10.0, 100.0)
_FEATURE_SETS = (
    ("length", (FEATURE_GRAPHEME_COUNT,)),
    ("function", (FEATURE_FUNCTION_UNIT,)),
    ("length+function", (FEATURE_GRAPHEME_COUNT, FEATURE_FUNCTION_UNIT)),
)


@dataclass(slots=True)
class _WordAggregate:
    word_id: str
    passage_id: int
    word_number: int
    interest_area: int
    word: str
    participant_ids: set[str]
    fixation_count: int = 0

    @property
    def target(self) -> float:
        return self.fixation_count / len(self.participant_ids)


@dataclass(frozen=True, slots=True)
class ProvoCorpus:
    examples: tuple[SalienceExample, ...]
    participant_count: int
    source_row_count: int
    source_word_count: int
    excluded_word_count: int

    @property
    def unit_count(self) -> int:
        return sum(len(example.units) for example in self.examples)

    @property
    def passage_count(self) -> int:
        return len(self.examples)


@dataclass(frozen=True, slots=True)
class ProvoTrainingResult:
    artifact: bytes
    fitted: FittedLinearSalience
    report: dict[str, object]


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _integer(value: str, field: str) -> int:
    try:
        result = int(value)
    except (TypeError, ValueError) as error:
        raise DatasetError(f"{field} must be an integer") from error
    if result <= 0:
        raise DatasetError(f"{field} must be positive")
    return result


def _was_fixated(value: str, field: str) -> bool:
    try:
        skip = float(value)
    except (TypeError, ValueError) as error:
        raise DatasetError(f"{field} must be zero or one") from error
    if skip not in (0.0, 1.0):
        raise DatasetError(f"{field} must be zero or one")
    return skip == 0.0


def _load_words(path: Path) -> tuple[tuple[_WordAggregate, ...], int, int, int]:
    words: dict[tuple[int, int], _WordAggregate] = {}
    participants: set[str] = set()
    source_rows = 0
    with path.open(newline="", encoding="utf-8-sig") as source:
        reader = csv.DictReader(source)
        columns = frozenset(reader.fieldnames or ())
        missing = sorted(_REQUIRED_COLUMNS - columns)
        if missing:
            raise DatasetError(f"Provo CSV is missing columns: {', '.join(missing)}")
        for row_number, row in enumerate(reader, 2):
            source_rows += 1
            participant = row["Participant_ID"].strip()
            if not participant or participant == "NA":
                raise DatasetError(f"row {row_number}.Participant_ID is invalid")
            participants.add(participant)
            word_id = row["Word_Unique_ID"].strip()
            if word_id == "NA":
                continue
            passage_id = _integer(row["Text_ID"], f"row {row_number}.Text_ID")
            word_number = _integer(row["Word_Number"], f"row {row_number}.Word_Number")
            interest_area = _integer(row["IA_ID"], f"row {row_number}.IA_ID")
            word = row["Word_Cleaned"].strip()
            if not word:
                raise DatasetError(f"row {row_number}.Word_Cleaned is empty")
            key = (passage_id, interest_area)
            aggregate = words.get(key)
            if aggregate is None:
                aggregate = _WordAggregate(
                    word_id, passage_id, word_number, interest_area, word, set()
                )
                words[key] = aggregate
            elif (aggregate.word_id, aggregate.word_number, aggregate.word) != (
                word_id,
                word_number,
                word,
            ):
                raise DatasetError(
                    f"row {row_number} changes metadata for passage {passage_id} "
                    f"interest area {interest_area}"
                )
            if participant in aggregate.participant_ids:
                raise DatasetError(
                    f"row {row_number} duplicates participant {participant!r} for {word_id!r}"
                )
            aggregate.participant_ids.add(participant)
            aggregate.fixation_count += int(
                _was_fixated(row["IA_SKIP"], f"row {row_number}.IA_SKIP")
            )
    ordered = tuple(
        sorted(words.values(), key=lambda item: (item.passage_id, item.interest_area))
    )
    compatible = tuple(item for item in ordered if _SINGLE_RUNTIME_UNIT(item.word))
    return compatible, len(participants), source_rows, len(ordered)


def _analyze_words(
    words: Sequence[_WordAggregate], analyzer: str | Path
) -> tuple[SalienceUnit, ...]:
    if not words:
        raise DatasetError("Provo CSV contains no runtime-compatible words")
    process = subprocess.run(
        (str(analyzer), "--language", "en", "--dump-analysis"),
        input=" ".join(word.word for word in words),
        text=True,
        encoding="utf-8",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if process.returncode != 0:
        detail = process.stderr.strip() or f"exit status {process.returncode}"
        raise ValueError(f"LAE analyzer failed: {detail}")
    try:
        analysis = json.loads(process.stdout)
        nodes = analysis["nodes"]
        runtime_units = [node for node in nodes if node.get("kind") == "unit"]
    except (json.JSONDecodeError, KeyError, TypeError) as error:
        raise ValueError("LAE analyzer did not emit valid analysis JSON") from error
    if len(runtime_units) != len(words):
        raise ValueError(
            "LAE analyzer unit count does not match the compatible Provo word count"
        )
    result: list[SalienceUnit] = []
    for word, node in zip(words, runtime_units):
        try:
            features = {
                int(item["id"]): float(item["value"]) for item in node["features"]
            }
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError("LAE analyzer emitted an invalid unit feature") from error
        graphemes = features.get(FEATURE_GRAPHEME_COUNT)
        if graphemes != float(len(word.word)):
            raise ValueError(f"LAE analyzer disagrees with Provo token {word.word_id!r}")
        content = features.get(FEATURE_CONTENT_UNIT, 0.0) == 1.0
        function = features.get(FEATURE_FUNCTION_UNIT, 0.0) == 1.0
        if content == function:
            raise ValueError("LAE analyzer must classify every English unit exactly once")
        result.append(
            SalienceUnit(
                (
                    SalienceFeature(FEATURE_GRAPHEME_COUNT, graphemes),
                    SalienceFeature(FEATURE_FUNCTION_UNIT, float(function)),
                ),
                word.target,
            )
        )
    return tuple(result)


def load_provo_corpus(
    csv_path: str | Path,
    analyzer: str | Path,
    *,
    expected_sha256: str | None = PROVO_SHA256,
) -> ProvoCorpus:
    """Load canonical Provo observations and snapshot exact runtime features."""

    path = Path(csv_path)
    if expected_sha256 is not None:
        actual = _sha256(path)
        if actual != expected_sha256.lower():
            raise DatasetError(
                f"Provo CSV SHA-256 mismatch: expected {expected_sha256}, got {actual}"
            )
    words, participant_count, source_rows, source_word_count = _load_words(path)
    units = _analyze_words(words, analyzer)
    examples: list[SalienceExample] = []
    index = 0
    for passage_id in sorted({word.passage_id for word in words}):
        count = sum(word.passage_id == passage_id for word in words)
        examples.append(
            SalienceExample(
                f"provo-passage-{passage_id}", "en", units[index : index + count]
            )
        )
        index += count
    return ProvoCorpus(
        tuple(examples),
        participant_count,
        source_rows,
        source_word_count,
        source_word_count - len(words),
    )


def _flatten(examples: Iterable[SalienceExample]) -> tuple[SalienceUnit, ...]:
    return tuple(unit for example in examples for unit in example.units)


def _metrics_dict(metrics: LinearSalienceMetrics) -> dict[str, int | float | None]:
    return metrics.as_dict()


def _cross_validate(
    examples: Sequence[SalienceExample],
    feature_ids: tuple[int, ...],
    ridge: float,
    folds: int,
) -> LinearSalienceMetrics:
    passage_folds = {
        example.identifier: index % folds for index, example in enumerate(examples)
    }
    held_out_units: list[SalienceUnit] = []
    held_out_predictions: list[float] = []
    for fold in range(folds):
        training = tuple(
            example for example in examples if passage_folds[example.identifier] != fold
        )
        held_out = tuple(
            example for example in examples if passage_folds[example.identifier] == fold
        )
        fitted = fit_linear_salience(training, feature_ids, ridge)
        units = _flatten(held_out)
        held_out_units.extend(units)
        held_out_predictions.extend(fitted.predict(unit) for unit in units)
    return linear_salience_metrics(held_out_units, held_out_predictions)


def _baseline(examples: Sequence[SalienceExample], folds: int) -> LinearSalienceMetrics:
    passage_folds = {
        example.identifier: index % folds for index, example in enumerate(examples)
    }
    held_out_units: list[SalienceUnit] = []
    held_out_predictions: list[float] = []
    for fold in range(folds):
        training_units = _flatten(
            example for example in examples if passage_folds[example.identifier] != fold
        )
        held_out = _flatten(
            example for example in examples if passage_folds[example.identifier] == fold
        )
        mean = sum(unit.target_salience for unit in training_units) / len(training_units)
        held_out_units.extend(held_out)
        held_out_predictions.extend(mean for _ in held_out)
    return linear_salience_metrics(held_out_units, held_out_predictions)


def train_provo_model(
    csv_path: str | Path,
    analyzer: str | Path,
    *,
    model_version: int = 1,
    folds: int = 5,
    expected_sha256: str | None = PROVO_SHA256,
) -> ProvoTrainingResult:
    """Select on passage-held-out folds, refit all data, and compile a `.lem`."""

    if folds < 2:
        raise ValueError("Provo training requires at least two folds")
    corpus = load_provo_corpus(csv_path, analyzer, expected_sha256=expected_sha256)
    if folds > corpus.passage_count:
        raise ValueError("fold count exceeds Provo passage count")
    candidates: list[tuple[str, tuple[int, ...], float, LinearSalienceMetrics]] = []
    for name, feature_ids in _FEATURE_SETS:
        for ridge in _RIDGES:
            metrics = _cross_validate(corpus.examples, feature_ids, ridge, folds)
            candidates.append((name, feature_ids, ridge, metrics))
    selected = min(
        candidates,
        key=lambda item: (
            item[3].root_mean_square_error,
            item[3].mean_absolute_error,
            len(item[1]),
            item[2],
            item[0],
        ),
    )
    selected_name, selected_features, selected_ridge, validation = selected
    fitted = fit_linear_salience(corpus.examples, selected_features, selected_ridge)
    artifact = build_linear_salience_artifact(
        fitted.bias,
        fitted.weights,
        languages=("en",),
        model_version=model_version,
    )
    baseline = _baseline(corpus.examples, folds)
    report: dict[str, object] = {
        "schema_version": 1,
        "model": {
            "name": "lae-provo-fixation-v1",
            "model_version": model_version,
            "type": "linear_unit_salience",
            "language": "en",
            "target": "probability that a reader fixates the word",
            "recommended_binary_threshold": 0.60,
            "selection": selected_name,
            "ridge": selected_ridge,
            "bias": fitted.bias,
            "weights": [
                {
                    "feature_id": feature_id,
                    "feature": FEATURE_NAMES[feature_id],
                    "weight": weight,
                }
                for feature_id, weight in fitted.weights
            ],
        },
        "data": {
            "name": "The Provo Corpus",
            "project_url": PROVO_PROJECT_URL,
            "download_url": PROVO_DATA_URL,
            "paper_doi": PROVO_DOI,
            "license": PROVO_LICENSE,
            "license_url": PROVO_LICENSE_URL,
            "source_file": Path(csv_path).name,
            "source_sha256": _sha256(Path(csv_path)),
            "participant_count": corpus.participant_count,
            "passage_count": corpus.passage_count,
            "source_row_count": corpus.source_row_count,
            "source_word_count": corpus.source_word_count,
            "training_unit_count": corpus.unit_count,
            "excluded_word_count": corpus.excluded_word_count,
        },
        "evaluation": {
            "protocol": f"{folds}-fold passage-grouped cross-validation",
            "selection_metric": "root_mean_square_error",
            "baseline": _metrics_dict(baseline),
            "selected": _metrics_dict(validation),
            "relative_rmse_reduction": 1.0
            - validation.root_mean_square_error / baseline.root_mean_square_error,
            "candidates": [
                {
                    "features": name,
                    "feature_ids": list(feature_ids),
                    "ridge": ridge,
                    "metrics": _metrics_dict(metrics),
                }
                for name, feature_ids, ridge, metrics in candidates
            ],
            "all_data_fit": _metrics_dict(fitted.metrics),
        },
        "artifact": {
            "format": f"{FORMAT_VERSION[0]}.{FORMAT_VERSION[1]}",
            "minimum_runtime_abi": f"{ABI_VERSION >> 16}.{ABI_VERSION & 0xFFFF}",
            "size": len(artifact),
            "sha256": hashlib.sha256(artifact).hexdigest(),
        },
    }
    return ProvoTrainingResult(artifact, fitted, report)
