"""Command-line entry point for the LAE training skeleton."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict
from pathlib import Path

from .artifacts import (
    build_linear_salience_artifact,
    build_prefix_artifact,
    write_artifact,
)
from .dataset import DatasetError, JsonlDataset
from .evaluation import PrefixCandidate, evaluate_prefix
from .evaluation_records import EvaluationDataError
from .features import extract_features
from .linear_salience import fit_linear_salience
from .optimize import fit_prefix
from .plan_evaluation import PlanJsonlDataset, aggregate_plans, compare_plans
from .provo import train_provo_model
from .study_evaluation import StudyJsonlDataset, aggregate_study, compare_study
from .salience_dataset import SalienceJsonlDataset


def _dataset(path: str) -> tuple:
    return tuple(JsonlDataset(path))


def _candidate(arguments: argparse.Namespace) -> PrefixCandidate:
    if arguments.fixed is not None:
        return PrefixCandidate("fixed", fixed_graphemes=arguments.fixed)
    return PrefixCandidate("proportional", proportion=arguments.proportion)


def _languages(arguments: argparse.Namespace, examples: tuple) -> tuple[str, ...]:
    if arguments.language:
        return tuple(arguments.language)
    return tuple(sorted({example.language for example in examples}, key=str.lower))


def _extract(arguments: argparse.Namespace) -> None:
    rows = extract_features(_dataset(arguments.dataset))
    output = Path(arguments.output).open("w", encoding="utf-8") if arguments.output else None
    stream = output if output is not None else sys.stdout
    try:
        for row in rows:
            print(json.dumps(asdict(row), sort_keys=True, ensure_ascii=False), file=stream)
    finally:
        if output is not None:
            output.close()


def _evaluate(arguments: argparse.Namespace) -> None:
    rows = tuple(extract_features(_dataset(arguments.dataset)))
    candidate = _candidate(arguments)
    report = {
        "candidate": asdict(candidate),
        "metrics": evaluate_prefix(rows, candidate).as_dict(),
    }
    print(json.dumps(report, sort_keys=True))


def _fit(arguments: argparse.Namespace) -> None:
    examples = _dataset(arguments.dataset)
    rows = tuple(extract_features(examples))
    fitted = fit_prefix(rows, arguments.strategy)
    languages = _languages(arguments, examples)
    artifact = build_prefix_artifact(
        fitted.candidate,
        languages=languages,
        model_version=arguments.model_version,
    )
    write_artifact(arguments.output, artifact)
    report = {
        "artifact": str(arguments.output),
        "artifact_size": len(artifact),
        "candidate": asdict(fitted.candidate),
        "candidates_evaluated": fitted.candidates_evaluated,
        "languages": languages,
        "metrics": fitted.metrics.as_dict(),
    }
    print(json.dumps(report, sort_keys=True))


def _fit_linear_salience(arguments: argparse.Namespace) -> None:
    examples = tuple(SalienceJsonlDataset(arguments.dataset))
    fitted = fit_linear_salience(examples, arguments.feature, arguments.ridge)
    languages = (
        tuple(arguments.language)
        if arguments.language
        else tuple(sorted({example.language for example in examples}, key=str.lower))
    )
    artifact = build_linear_salience_artifact(
        fitted.bias,
        fitted.weights,
        languages=languages,
        model_version=arguments.model_version,
    )
    write_artifact(arguments.output, artifact)
    report = {
        "artifact": str(arguments.output),
        "artifact_size": len(artifact),
        "bias": fitted.bias,
        "languages": languages,
        "metrics": fitted.metrics.as_dict(),
        "ridge": fitted.ridge,
        "weights": [
            {"feature": feature_id, "weight": weight}
            for feature_id, weight in fitted.weights
        ],
    }
    print(json.dumps(report, sort_keys=True))


def _train_provo(arguments: argparse.Namespace) -> None:
    result = train_provo_model(
        arguments.dataset,
        arguments.analyzer,
        model_version=arguments.model_version,
        folds=arguments.folds,
    )
    write_artifact(arguments.output, result.artifact)
    if arguments.report:
        Path(arguments.report).write_text(
            json.dumps(result.report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(json.dumps(result.report, sort_keys=True))


def _summarize_plans(arguments: argparse.Namespace) -> None:
    records = tuple(PlanJsonlDataset(arguments.plans))
    report = {
        "metrics": [item.as_dict() for item in aggregate_plans(records)],
        "comparisons": [
            item.as_dict() for item in compare_plans(records, arguments.baseline)
        ],
    }
    print(json.dumps(report, sort_keys=True))


def _summarize_study(arguments: argparse.Namespace) -> None:
    observations = tuple(StudyJsonlDataset(arguments.study))
    report = {
        "metrics": [item.as_dict() for item in aggregate_study(observations)],
        "comparisons": [
            item.as_dict() for item in compare_study(observations, arguments.baseline)
        ],
    }
    print(json.dumps(report, sort_keys=True))


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(prog="lae-train")
    commands = result.add_subparsers(dest="command", required=True)

    extract = commands.add_parser("extract-features", help="emit one JSON feature row per unit")
    extract.add_argument("dataset")
    extract.add_argument("--output")
    extract.set_defaults(handler=_extract)

    evaluate = commands.add_parser("evaluate-prefix", help="score one prefix candidate")
    evaluate.add_argument("dataset")
    selection = evaluate.add_mutually_exclusive_group(required=True)
    selection.add_argument("--fixed", type=int)
    selection.add_argument("--proportion", type=float)
    evaluate.set_defaults(handler=_evaluate)

    fit = commands.add_parser("fit-prefix", help="optimize and export a prefix artifact")
    fit.add_argument("dataset")
    fit.add_argument("output")
    fit.add_argument("--strategy", choices=("auto", "fixed", "proportional"), default="auto")
    fit.add_argument("--language", action="append")
    fit.add_argument("--model-version", type=int, default=1)
    fit.set_defaults(handler=_fit)

    learned = commands.add_parser(
        "fit-linear-salience", help="fit and export a linear unit-salience model"
    )
    learned.add_argument("dataset")
    learned.add_argument("output")
    learned.add_argument("--feature", type=int, action="append")
    learned.add_argument("--ridge", type=float, default=1e-6)
    learned.add_argument("--language", action="append")
    learned.add_argument("--model-version", type=int, default=1)
    learned.set_defaults(handler=_fit_linear_salience)

    provo = commands.add_parser(
        "train-provo", help="train and validate the real-data English fixation model"
    )
    provo.add_argument("dataset", help="canonical Provo eye-tracking CSV")
    provo.add_argument("output", help="output .lem artifact")
    provo.add_argument("--analyzer", required=True, help="path to the matching le-cli")
    provo.add_argument("--report", help="write the full reproducibility report as JSON")
    provo.add_argument("--folds", type=int, default=5)
    provo.add_argument("--model-version", type=int, default=1)
    provo.set_defaults(handler=_train_provo)

    plans = commands.add_parser(
        "summarize-plans", help="aggregate strategy-neutral offline plan metrics"
    )
    plans.add_argument("plans")
    plans.add_argument("--baseline", default="plain")
    plans.set_defaults(handler=_summarize_plans)

    study = commands.add_parser(
        "summarize-study", help="aggregate outcomes and paired A/B comparisons"
    )
    study.add_argument("study")
    study.add_argument("--baseline", default="plain")
    study.set_defaults(handler=_summarize_study)
    return result


def main(argv: list[str] | None = None) -> int:
    arguments = parser().parse_args(argv)
    try:
        arguments.handler(arguments)
        return 0
    except (DatasetError, EvaluationDataError, OSError, ValueError) as error:
        parser().error(str(error))
    return 2
