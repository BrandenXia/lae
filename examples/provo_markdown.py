#!/usr/bin/env python3
"""Render English text as Markdown bold with the released Provo fixation model."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys

from lae import ProcessOptions, Runtime


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MODEL = ROOT / "models" / "lae-provo-fixation-v1.lem"


def _runtime_path(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    if os.environ.get("LAE_RUNTIME"):
        return None
    for directory in (ROOT / "build-shared", ROOT / "build"):
        for name in ("lible_runtime.dylib", "lible_runtime.so", "le_runtime.dll"):
            matches = sorted(
                directory.glob(f"**/{name}"),
                key=lambda path: path.stat().st_mtime,
                reverse=True,
            )
            if matches:
                return str(matches[0])
    return None


def _markdown(source: str, spans: tuple) -> str:
    encoded = source.encode("utf-8")
    output = bytearray()
    cursor = 0
    for emphasis in spans:
        begin, end = emphasis.span.begin, emphasis.span.end
        output.extend(encoded[cursor:begin])
        output.extend(b"**")
        output.extend(encoded[begin:end])
        output.extend(b"**")
        cursor = end
    output.extend(encoded[cursor:])
    return output.decode("utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paragraph", nargs="?", help="text; reads stdin when omitted")
    parser.add_argument("--library", help="path to the LAE shared runtime")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument(
        "--threshold",
        type=float,
        default=0.60,
        help="minimum predicted fixation probability (default: 0.60)",
    )
    arguments = parser.parse_args()
    paragraph = (
        arguments.paragraph
        if arguments.paragraph is not None
        else sys.stdin.read().rstrip("\n")
    )
    with Runtime(_runtime_path(arguments.library)) as runtime:
        with runtime.load_model_file(arguments.model) as model:
            emphasis = runtime.process(
                paragraph,
                ProcessOptions(language="en", salience_threshold=arguments.threshold),
                model=model,
            )
    print(_markdown(paragraph, emphasis))


if __name__ == "__main__":
    main()
