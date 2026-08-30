from __future__ import annotations

import os
import struct
import unittest
import zlib

from lae import (
    LaeError,
    ModelType,
    PrefixStrategy,
    ProcessOptions,
    ReadingModel,
    Runtime,
    TextSpan,
)


def prefix_artifact() -> bytes:
    language = b"en"
    language_table = struct.pack("<H", len(language)) + language
    parameters = struct.pack("<IIf", int(PrefixStrategy.FIXED), 1, 0.5)
    parameter_offset = 64 + len(language_table)
    total_size = parameter_offset + len(parameters)
    header = struct.pack(
        "<8sHH13I",
        b"LAEMODL\0",
        1,
        0,
        64,
        total_size,
        0,
        (1 << 16) | 7,
        int(ModelType.PREFIX),
        7,
        1,
        0,
        64,
        parameter_offset,
        parameter_offset,
        3,
        0,
    )
    result = bytearray(header + language_table + parameters)
    struct.pack_into("<I", result, 20, zlib.crc32(result) & 0xFFFFFFFF)
    return bytes(result)


class RuntimeBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.library = os.environ["LAE_RUNTIME"]

    def test_default_process_preserves_utf8_byte_offsets(self) -> None:
        with Runtime(self.library) as runtime:
            plan = runtime.process("éclair")
        self.assertEqual([item.span for item in plan], [TextSpan(0, 4)])
        self.assertEqual(plan[0].strength, 1.0)

    def test_lexical_core_model_uses_language_provider(self) -> None:
        options = ProcessOptions(language="en", reading_model=ReadingModel.LEXICAL_CORE)
        with Runtime(self.library) as runtime:
            plan = runtime.process("unbelievable reading", options)
        self.assertEqual(
            [item.span for item in plan],
            [TextSpan(2, 8), TextSpan(13, 17)],
        )

    def test_artifact_metadata_and_processing(self) -> None:
        runtime = Runtime(self.library)
        model = runtime.load_model(prefix_artifact())
        try:
            self.assertEqual(model.type, ModelType.PREFIX)
            self.assertEqual(model.version, 7)
            self.assertEqual(model.minimum_abi, (1, 7))
            self.assertEqual(model.languages, ("en",))
            self.assertEqual(model.required_features, ())
            self.assertTrue(model.supports_language("en-US"))
            plan = runtime.process(
                "reading", ProcessOptions(language="en"), model=model
            )
            self.assertEqual([item.span for item in plan], [TextSpan(0, 1)])
            runtime.close()
            self.assertEqual(model.languages, ("en",))
            with Runtime(self.library) as second_runtime:
                second_plan = second_runtime.process(
                    "reading", ProcessOptions(language="en"), model=model
                )
            self.assertEqual([item.span for item in second_plan], [TextSpan(0, 1)])
        finally:
            model.close()
            runtime.close()

    def test_native_diagnostic_is_exposed(self) -> None:
        with Runtime(self.library) as runtime:
            with self.assertRaises(LaeError) as caught:
                runtime.load_model(b"not a model")
        self.assertEqual(caught.exception.name, "LE_ERROR_MODEL_INVALID")
        self.assertTrue(caught.exception.detail)

    def test_closed_runtime_rejects_processing(self) -> None:
        runtime = Runtime(self.library)
        runtime.close()
        with self.assertRaisesRegex(RuntimeError, "runtime is closed"):
            runtime.process("reading")


if __name__ == "__main__":
    unittest.main()
