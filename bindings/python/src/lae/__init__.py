"""Python bindings for the LAE typographic emphasis runtime."""

from .runtime import (
    Emphasis,
    LaeError,
    LanguageRegion,
    Model,
    ModelType,
    PrefixStrategy,
    PresentationPolicy,
    ProcessOptions,
    ReadingModel,
    Runtime,
    TextSpan,
)

__version__ = "0.15.0"

__all__ = [
    "Emphasis",
    "LaeError",
    "LanguageRegion",
    "Model",
    "ModelType",
    "PrefixStrategy",
    "PresentationPolicy",
    "ProcessOptions",
    "ReadingModel",
    "Runtime",
    "TextSpan",
    "__version__",
]
