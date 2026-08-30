from __future__ import annotations

import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from lae_training.artifacts import (
    ABI_VERSION,
    build_lexical_core_artifact,
    build_prefix_artifact,
)
from lae_training.dataset import DatasetError, JsonlDataset, parse_record
from lae_training.evaluation import PrefixCandidate, evaluate_prefix
from lae_training.features import extract_features
from lae_training.optimize import fit_prefix


FIXTURE = Path(__file__).parent / "fixtures" / "prefix-training.jsonl"


class DatasetTests(unittest.TestCase):
    def test_jsonl_dataset_and_features(self) -> None:
        examples = tuple(JsonlDataset(FIXTURE))
        self.assertEqual([example.identifier for example in examples], ["english-1", "english-2"])
        rows = tuple(extract_features(examples))
        self.assertEqual(len(rows), 4)
        self.assertEqual(rows[0].utf8_bytes, 4)
        self.assertEqual(rows[0].graphemes, 4)
        self.assertEqual(rows[0].target_density, 0.5)
        self.assertEqual(rows[0].alphabetic_fraction, 1.0)

    def test_non_ascii_byte_offsets_are_preserved(self) -> None:
        example = parse_record(
            {
                "schema_version": 1,
                "id": "han",
                "text": "研究",
                "language": "zh-Hans",
                "units": [
                    {
                        "begin": 0,
                        "end": 6,
                        "grapheme_boundaries": [0, 3, 6],
                        "target_prefix_graphemes": 1,
                    }
                ],
            }
        )
        row = next(extract_features((example,)))
        self.assertEqual((row.utf8_bytes, row.code_points, row.graphemes), (6, 2, 2))

    def test_utf8_split_is_rejected(self) -> None:
        with self.assertRaisesRegex(DatasetError, "UTF-8"):
            parse_record(
                {
                    "schema_version": 1,
                    "id": "bad",
                    "text": "研",
                    "language": "zh",
                    "units": [
                        {
                            "begin": 0,
                            "end": 2,
                            "grapheme_boundaries": [0, 2],
                            "target_prefix_graphemes": 1,
                        }
                    ],
                }
            )

    def test_duplicate_ids_are_rejected(self) -> None:
        line = FIXTURE.read_text(encoding="utf-8").splitlines()[0]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "duplicate.jsonl"
            path.write_text(f"{line}\n{line}\n", encoding="utf-8")
            with self.assertRaisesRegex(DatasetError, "repeats id"):
                tuple(JsonlDataset(path))


class OptimizationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.rows = tuple(extract_features(JsonlDataset(FIXTURE)))

    def test_fit_selects_exact_fixed_candidate(self) -> None:
        fitted = fit_prefix(self.rows)
        self.assertEqual(fitted.candidate, PrefixCandidate("fixed", fixed_graphemes=2))
        self.assertEqual(fitted.metrics.mean_absolute_error, 0.0)
        self.assertEqual(fitted.metrics.exact_match_rate, 1.0)
        self.assertGreater(fitted.candidates_evaluated, 1)

    def test_evaluation_reports_density_and_error(self) -> None:
        metrics = evaluate_prefix(self.rows, PrefixCandidate("fixed", fixed_graphemes=1))
        self.assertEqual(metrics.mean_absolute_error, 1.0)
        self.assertEqual(metrics.exact_match_rate, 0.0)
        self.assertLess(metrics.predicted_density, metrics.target_density)

    def test_proportions_use_runtime_binary32_precision(self) -> None:
        candidate = PrefixCandidate("proportional", proportion=0.2)
        self.assertEqual(candidate.predict(5), 2)


class ArtifactTests(unittest.TestCase):
    def test_prefix_encoder_matches_runtime_golden_artifact(self) -> None:
        artifact = build_prefix_artifact(
            PrefixCandidate("fixed", fixed_graphemes=2, proportion=0.5),
            languages=("en", "zh"),
            model_version=7,
        )
        expected = bytes.fromhex(
            "4c41454d4f444c000100000040000000540000000b4c30e10500010001000000"
            "0700000002000000000000004000000048000000480000000300000000000000"
            "0200656e02007a6802000000020000000000003f"
        )
        self.assertEqual(artifact, expected)
        self.assertEqual(len(artifact), 84)
        self.assertEqual(artifact[:8], b"LAEMODL\0")
        self.assertEqual(struct.unpack_from("<I", artifact, 20)[0], 0xE1304C0B)
        checksum_input = bytearray(artifact)
        struct.pack_into("<I", checksum_input, 20, 0)
        self.assertEqual(zlib.crc32(checksum_input) & 0xFFFFFFFF, 0xE1304C0B)
        self.assertEqual(struct.unpack_from("<I", artifact, 24)[0], ABI_VERSION)

    def test_lexical_core_encoder_declares_required_feature(self) -> None:
        artifact = build_lexical_core_artifact(languages=("en",), model_version=3)
        feature_offset = struct.unpack_from("<I", artifact, 48)[0]
        self.assertEqual(struct.unpack_from("<I", artifact, feature_offset)[0], 0x00010001)

    def test_duplicate_languages_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate"):
            build_prefix_artifact(
                PrefixCandidate("fixed", fixed_graphemes=2), languages=("en", "EN")
            )


if __name__ == "__main__":
    unittest.main()
