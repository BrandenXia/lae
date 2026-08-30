"""Offline training interfaces for LAE runtime artifacts."""

from .artifacts import (
    ABI_VERSION,
    FORMAT_VERSION,
    build_lexical_core_artifact,
    build_linear_salience_artifact,
    build_prefix_artifact,
    write_artifact,
)
from .dataset import Dataset, DatasetError, JsonlDataset, TrainingExample, UnitTarget
from .evaluation import PrefixCandidate, PrefixMetrics, evaluate_prefix
from .evaluation_records import EvaluationDataError
from .features import UnitFeatures, extract_features
from .linear_salience import (
    FittedLinearSalience,
    LinearSalienceMetrics,
    fit_linear_salience,
)
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
from .salience_dataset import (
    SalienceExample,
    SalienceFeature,
    SalienceJsonlDataset,
    SalienceUnit,
)

__all__ = [
    "ABI_VERSION",
    "FORMAT_VERSION",
    "Dataset",
    "DatasetError",
    "EvaluationDataError",
    "FittedPrefix",
    "FittedLinearSalience",
    "JsonlDataset",
    "PrefixCandidate",
    "PrefixMetrics",
    "PlanComparison",
    "PlanJsonlDataset",
    "PlanMetrics",
    "PlanRecord",
    "LinearSalienceMetrics",
    "SalienceExample",
    "SalienceFeature",
    "SalienceJsonlDataset",
    "SalienceUnit",
    "StudyComparison",
    "StudyJsonlDataset",
    "StudyMetrics",
    "StudyObservation",
    "TrainingExample",
    "UnitFeatures",
    "UnitTarget",
    "build_lexical_core_artifact",
    "build_linear_salience_artifact",
    "build_prefix_artifact",
    "aggregate_plans",
    "aggregate_study",
    "compare_plans",
    "compare_study",
    "evaluate_prefix",
    "extract_features",
    "fit_prefix",
    "fit_linear_salience",
    "write_artifact",
]
