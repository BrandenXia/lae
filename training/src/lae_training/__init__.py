"""Offline training interfaces for LAE runtime artifacts."""

from .artifacts import (
    ABI_VERSION,
    FORMAT_VERSION,
    build_lexical_core_artifact,
    build_prefix_artifact,
    write_artifact,
)
from .dataset import Dataset, DatasetError, JsonlDataset, TrainingExample, UnitTarget
from .evaluation import PrefixCandidate, PrefixMetrics, evaluate_prefix
from .evaluation_records import EvaluationDataError
from .features import UnitFeatures, extract_features
from .optimize import FittedPrefix, fit_prefix
from .plan_evaluation import (
    PlanComparison,
    PlanJsonlDataset,
    PlanMetrics,
    PlanRecord,
    aggregate_plans,
    compare_plans,
)
from .study_evaluation import (
    StudyComparison,
    StudyJsonlDataset,
    StudyMetrics,
    StudyObservation,
    aggregate_study,
    compare_study,
)

__all__ = [
    "ABI_VERSION",
    "FORMAT_VERSION",
    "Dataset",
    "DatasetError",
    "EvaluationDataError",
    "FittedPrefix",
    "JsonlDataset",
    "PrefixCandidate",
    "PrefixMetrics",
    "PlanComparison",
    "PlanJsonlDataset",
    "PlanMetrics",
    "PlanRecord",
    "StudyComparison",
    "StudyJsonlDataset",
    "StudyMetrics",
    "StudyObservation",
    "TrainingExample",
    "UnitFeatures",
    "UnitTarget",
    "build_lexical_core_artifact",
    "build_prefix_artifact",
    "aggregate_plans",
    "aggregate_study",
    "compare_plans",
    "compare_study",
    "evaluate_prefix",
    "extract_features",
    "fit_prefix",
    "write_artifact",
]
