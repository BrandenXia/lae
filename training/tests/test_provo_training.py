from __future__ import annotations

import csv
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from lae_training.dataset import DatasetError
from lae_training.artifacts import build_linear_salience_artifact
from lae_training.provo import (
    FEATURE_FUNCTION_UNIT,
    FEATURE_GRAPHEME_COUNT,
    load_provo_corpus,
    train_provo_model,
)
from lae_training.salience_dataset import SalienceFeature, SalienceUnit


class ProvoTrainingTests(unittest.TestCase):
    def _dataset(self, directory: str) -> Path:
        path = Path(directory) / "provo.csv"
        fields = (
            "Participant_ID",
            "Word_Unique_ID",
            "Text_ID",
            "Word_Number",
            "Word_Cleaned",
            "IA_ID",
            "IA_SKIP",
        )
        words = (
            (1, "the", (0, 1)),
            (1, "planet", (0, 0)),
            (1, "and", (1, 1)),
            (2, "read", (0, 1)),
            (2, "complexity", (0, 0)),
            (2, "to", (1, 1)),
            (3, "a", (1, 1)),
            (3, "language", (0, 0)),
            (3, "model", (0, 1)),
            (4, "with", (1, 1)),
            (4, "useful", (0, 0)),
            (4, "signals", (0, 1)),
        )
        with path.open("w", newline="", encoding="utf-8") as output:
            writer = csv.DictWriter(output, fieldnames=fields)
            writer.writeheader()
            word_id = 0
            passage_positions: dict[int, int] = {}
            for passage_id, word, skips in words:
                word_id += 1
                passage_positions[passage_id] = passage_positions.get(passage_id, 0) + 1
                for participant, skip in zip(("P1", "P2"), skips):
                    writer.writerow(
                        {
                            "Participant_ID": participant,
                            "Word_Unique_ID": f"W{word_id}",
                            "Text_ID": passage_id,
                            "Word_Number": passage_positions[passage_id],
                            "Word_Cleaned": word,
                            "IA_ID": passage_positions[passage_id],
                            "IA_SKIP": skip,
                        }
                    )
        return path

    @staticmethod
    def _runtime_units(words, analyzer) -> tuple[SalienceUnit, ...]:
        function_words = {"a", "and", "the", "to", "with"}
        return tuple(
            SalienceUnit(
                (
                    SalienceFeature(FEATURE_GRAPHEME_COUNT, float(len(word.word))),
                    SalienceFeature(
                        FEATURE_FUNCTION_UNIT, float(word.word in function_words)
                    ),
                ),
                word.target,
            )
            for word in words
        )

    def test_loads_fixation_probability_by_passage(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = self._dataset(directory)
            with patch("lae_training.provo._analyze_words", self._runtime_units):
                corpus = load_provo_corpus(path, "unused", expected_sha256=None)
        self.assertEqual(corpus.participant_count, 2)
        self.assertEqual(corpus.passage_count, 4)
        self.assertEqual(corpus.unit_count, 12)
        self.assertEqual(corpus.examples[0].units[0].target_salience, 0.5)
        self.assertEqual(corpus.examples[0].units[1].target_salience, 1.0)

    def test_trains_cross_validated_runtime_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = self._dataset(directory)
            with patch("lae_training.provo._analyze_words", self._runtime_units):
                result = train_provo_model(
                    path, "unused", folds=2, expected_sha256=None
                )
        self.assertEqual(result.artifact[:8], b"LAEMODL\0")
        self.assertEqual(result.report["data"]["training_unit_count"], 12)
        self.assertEqual(
            result.report["evaluation"]["protocol"],
            "2-fold passage-grouped cross-validation",
        )
        self.assertEqual(result.report["artifact"]["size"], len(result.artifact))

    def test_rejects_noncanonical_checksum(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = self._dataset(directory)
            with self.assertRaisesRegex(DatasetError, "SHA-256 mismatch"):
                load_provo_corpus(path, "unused")

    def test_checked_in_release_matches_its_report(self) -> None:
        root = Path(__file__).parents[2]
        artifact = (root / "models" / "lae-provo-fixation-v1.lem").read_bytes()
        report = json.loads(
            (root / "models" / "lae-provo-fixation-v1.json").read_text(
                encoding="utf-8"
            )
        )
        model = report["model"]
        rebuilt = build_linear_salience_artifact(
            model["bias"],
            tuple(
                (weight["feature_id"], weight["weight"])
                for weight in model["weights"]
            ),
            languages=(model["language"],),
            model_version=model["model_version"],
        )
        self.assertEqual(artifact, rebuilt)
        self.assertEqual(
            hashlib.sha256(artifact).hexdigest(), report["artifact"]["sha256"]
        )


if __name__ == "__main__":
    unittest.main()
