"""Command-line entry point for the LAE training skeleton."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict
from pathlib import Path

from .artifacts import build_prefix_artifact, write_artifact
from .dataset import DatasetError, JsonlDataset
from .evaluation import PrefixCandidate, evaluate_prefix
from .features import extract_features
from .optimize import fit_prefix


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
    return result


def main(argv: list[str] | None = None) -> int:
    arguments = parser().parse_args(argv)
    try:
        arguments.handler(arguments)
        return 0
    except (DatasetError, OSError, ValueError) as error:
        parser().error(str(error))
    return 2
