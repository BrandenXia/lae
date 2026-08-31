"""Python bindings for the LAE typographic emphasis runtime."""

from .runtime import (
    Emphasis,
    LaeError,
    Model,
    ModelType,
    PrefixStrategy,
    PresentationPolicy,
    ProcessOptions,
    ReadingModel,
    Runtime,
    TextSpan,
)

__version__ = "0.11.0"

__all__ = [
    "Emphasis",
    "LaeError",
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
