from __future__ import annotations

import struct
import unittest
from pathlib import Path

from lae_training.artifacts import build_linear_salience_artifact
from lae_training.dataset import DatasetError
from lae_training.linear_salience import fit_linear_salience
from lae_training.salience_dataset import SalienceJsonlDataset, parse_salience_record


FIXTURE = Path(__file__).parent / "fixtures" / "salience-training.jsonl"


class SalienceDatasetTests(unittest.TestCase):
    def test_loads_runtime_feature_snapshots(self) -> None:
        examples = tuple(SalienceJsonlDataset(FIXTURE))
        self.assertEqual(len(examples), 2)
        self.assertEqual(examples[0].units[1].feature_value(2), 2.0)
        self.assertEqual(examples[0].units[1].feature_value(0x00040001), 0.0)

    def test_unknown_runtime_feature_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            parse_salience_record(
                {
                    "schema_version": 1,
                    "id": "unknown-feature",
                    "language": "en",
                    "units": [
                        {
                            "features": [{"id": 0x7FFFFFFF, "value": 1}],
                            "target_salience": 0.5,
                        }
                    ],
                }
            )

    def test_japanese_script_features_are_supported(self) -> None:
        example = parse_salience_record(
            {
                "schema_version": 1,
                "id": "japanese-scripts",
                "language": "ja",
                "units": [
                    {
                        "features": [
                            {"id": 0x00040003, "value": 1},
                            {"id": 0x00040004, "value": 1},
                        ],
                        "target_salience": 0.5,
                    }
                ],
            }
        )
        self.assertEqual(
            tuple(feature.feature_id for feature in example.units[0].features),
            (0x00040003, 0x00040004),
        )

    def test_function_unit_feature_is_supported(self) -> None:
        example = parse_salience_record(
            {
                "schema_version": 1,
                "id": "function-unit",
                "language": "en",
                "units": [
                    {
                        "features": [{"id": 0x00030002, "value": 1}],
                        "target_salience": 0.1,
                    }
                ],
            }
        )
        self.assertEqual(example.units[0].feature_value(0x00030002), 1.0)

    def test_duplicate_features_are_rejected(self) -> None:
        with self.assertRaises(DatasetError):
            parse_salience_record(
                {
                    "schema_version": 1,
                    "id": "duplicate-feature",
                    "language": "en",
                    "units": [
                        {
                            "features": [{"id": 2, "value": 1}, {"id": 2, "value": 2}],
                            "target_salience": 0.5,
                        }
                    ],
                }
            )


class LinearSalienceTrainingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.examples = tuple(SalienceJsonlDataset(FIXTURE))

    def test_fits_deterministic_linear_model(self) -> None:
        fitted = fit_linear_salience(self.examples, ridge=0.0)
        self.assertEqual(tuple(feature for feature, _ in fitted.weights), (2,))
        self.assertAlmostEqual(fitted.bias, 0.0, places=6)
        self.assertAlmostEqual(fitted.weights[0][1], 0.2, places=6)
        self.assertLess(fitted.metrics.root_mean_square_error, 1e-6)
        self.assertAlmostEqual(fitted.metrics.r_squared or 0.0, 1.0, places=6)

    def test_exported_artifact_has_linear_model_layout(self) -> None:
        fitted = fit_linear_salience(self.examples, ridge=0.0)
        artifact = build_linear_salience_artifact(
            fitted.bias, fitted.weights, languages=("en",), model_version=4
        )
        self.assertEqual(len(artifact), 88)
        self.assertEqual(struct.unpack_from("<I", artifact, 28)[0], 3)
        self.assertEqual(struct.unpack_from("<I", artifact, 56)[0], 4)
        parameter_offset = struct.unpack_from("<I", artifact, 52)[0]
        self.assertEqual(struct.unpack_from("<I", artifact, parameter_offset + 4)[0], 1)

    def test_duplicate_export_weights_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate"):
            build_linear_salience_artifact(0.0, ((2, 0.2), (2, 0.3)))

    def test_export_cannot_claim_an_older_runtime_abi(self) -> None:
        with self.assertRaisesRegex(ValueError, "at least 1.6"):
            build_linear_salience_artifact(
                0.0, ((2, 0.2),), minimum_abi=(1 << 16) | 5
            )

    def test_function_feature_requires_abi_1_11(self) -> None:
        with self.assertRaisesRegex(ValueError, "1.11"):
            build_linear_salience_artifact(
                0.0, ((0x00030002, -0.4),), minimum_abi=(1 << 16) | 10
            )


if __name__ == "__main__":
    unittest.main()
