from __future__ import annotations

import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from lae_training.evaluation_records import EvaluationDataError
from lae_training.plan_evaluation import (
    PlanJsonlDataset,
    aggregate_plans,
    compare_plans,
    parse_plan_record,
)
from lae_training.study_evaluation import (
    StudyJsonlDataset,
    aggregate_study,
    compare_study,
    parse_study_observation,
)


FIXTURES = Path(__file__).parent / "fixtures"


class PlanEvaluationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.records = tuple(PlanJsonlDataset(FIXTURES / "plans.jsonl"))

    def test_aggregates_strategy_neutral_plan_metrics(self) -> None:
        metrics = {item.variant: item for item in aggregate_plans(self.records)}
        self.assertEqual(metrics["plain"].emphasis_density, 0.0)
        self.assertEqual(metrics["traditional-prefix"].emphasized_graphemes, 6)
        self.assertEqual(metrics["traditional-prefix"].emphasis_span_count, 3)
        self.assertAlmostEqual(metrics["traditional-prefix"].emphasis_density, 0.375)
        self.assertLess(
            metrics["morphological"].transitions_per_100_boundaries,
            metrics["traditional-prefix"].transitions_per_100_boundaries,
        )

    def test_compares_only_paired_examples(self) -> None:
        comparisons = {item.candidate: item for item in compare_plans(self.records, "plain")}
        prefix = comparisons["traditional-prefix"]
        self.assertEqual(prefix.paired_examples, 2)
        self.assertGreater(prefix.mean_emphasis_density_delta, 0.0)
        self.assertGreater(prefix.mean_transitions_per_100_boundaries_delta, 0.0)

    def test_comparison_allows_layout_density_to_change(self) -> None:
        records = tuple(
            replace(item, line_count=1)
            if item.variant == "traditional-prefix" and item.example_id == "example-1"
            else item
            for item in self.records
        )
        comparisons = {item.candidate: item for item in compare_plans(records, "plain")}
        self.assertGreater(
            comparisons["traditional-prefix"].mean_text_density_graphemes_per_line_delta,
            0.0,
        )

    def test_invalid_or_adjacent_ranges_are_rejected(self) -> None:
        with self.assertRaises(EvaluationDataError):
            parse_plan_record(
                {
                    "schema_version": 1,
                    "example_id": "example",
                    "variant": "candidate",
                    "grapheme_count": 5,
                    "line_count": 1,
                    "emphasized_ranges": [
                        {"begin": 0, "end": 2},
                        {"begin": 2, "end": 4},
                    ],
                }
            )

    def test_duplicate_example_variant_is_rejected(self) -> None:
        line = (FIXTURES / "plans.jsonl").read_text(encoding="utf-8").splitlines()[0]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "duplicates.jsonl"
            path.write_text(f"{line}\n{line}\n", encoding="utf-8")
            with self.assertRaisesRegex(EvaluationDataError, "repeats example"):
                tuple(PlanJsonlDataset(path))


class StudyEvaluationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.observations = tuple(StudyJsonlDataset(FIXTURES / "study.jsonl"))

    def test_aggregates_all_roadmap_measurements(self) -> None:
        metrics = {item.variant: item for item in aggregate_study(self.observations)}
        prefix = metrics["traditional-prefix"]
        self.assertEqual(prefix.observation_count, 2)
        self.assertEqual(prefix.participant_count, 2)
        self.assertEqual(prefix.fixation_duration_observation_count, 2)
        self.assertEqual(prefix.fixation_count_observation_count, 2)
        self.assertEqual(prefix.preference_observation_count, 2)
        self.assertAlmostEqual(prefix.mean_reading_speed_units_per_minute, 114.5454545)
        self.assertAlmostEqual(prefix.mean_comprehension_score, 0.8)
        self.assertAlmostEqual(prefix.mean_fixation_duration_ms or 0.0, 53000 / 190)
        self.assertAlmostEqual(prefix.fixation_count_per_100_content_units or 0.0, 95.0)
        self.assertAlmostEqual(prefix.regressions_per_100_content_units or 0.0, 9.0)
        self.assertAlmostEqual(prefix.mean_preference_score or 0.0, 0.675)
        self.assertAlmostEqual(prefix.mean_distraction_score or 0.0, 0.325)
        self.assertAlmostEqual(prefix.emphasis_density, 0.4)
        self.assertAlmostEqual(prefix.text_density_graphemes_per_line, 25.0)

    def test_paired_comparison_reports_candidate_minus_baseline(self) -> None:
        comparisons = {
            item.candidate: item for item in compare_study(self.observations, "plain")
        }
        morphological = comparisons["morphological"]
        self.assertEqual(morphological.paired_observations, 2)
        self.assertEqual(morphological.fixation_duration_pairs, 2)
        self.assertEqual(morphological.fixation_count_pairs, 2)
        self.assertEqual(morphological.distraction_pairs, 2)
        self.assertGreater(morphological.mean_reading_speed_delta, 0.0)
        self.assertGreater(morphological.mean_comprehension_delta, 0.0)
        self.assertLess(morphological.mean_fixation_duration_delta_ms or 0.0, 0.0)
        self.assertLess(morphological.mean_regressions_per_100_units_delta or 0.0, 0.0)
        self.assertGreater(morphological.mean_preference_delta or 0.0, 0.0)
        self.assertGreater(morphological.mean_distraction_delta or 0.0, 0.0)

    def test_study_comparison_allows_layout_density_to_change(self) -> None:
        observations = tuple(
            replace(item, line_count=10)
            if item.variant == "traditional-prefix"
            else item
            for item in self.observations
        )
        comparisons = {
            item.candidate: item for item in compare_study(observations, "plain")
        }
        self.assertGreater(
            comparisons[
                "traditional-prefix"
            ].mean_text_density_graphemes_per_line_delta,
            0.0,
        )

    def test_fixation_fields_must_appear_together(self) -> None:
        with self.assertRaisesRegex(EvaluationDataError, "appear together"):
            parse_study_observation(
                {
                    "schema_version": 1,
                    "participant_id": "p1",
                    "example_id": "e1",
                    "variant": "plain",
                    "duration_ms": 1000,
                    "content_unit_count": 10,
                    "grapheme_count": 20,
                    "line_count": 2,
                    "emphasized_graphemes": 0,
                    "comprehension_score": 1,
                    "fixation_count": 4,
                }
            )


if __name__ == "__main__":
    unittest.main()
