#include "le/analysis.h"
#include "le/model.h"
#include "le/reading.h"
#include "le/version.h"
#include "model/artifact.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 8U |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16U |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

void set_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

void reseal(std::vector<std::uint8_t>& bytes) {
    set_u32(bytes, 20, 0);
    set_u32(bytes, 20, le::model::checksum(bytes));
}

void expect_error(const std::vector<std::uint8_t>& bytes, le::model::ErrorKind kind,
                  std::string_view message) {
    try {
        static_cast<void>(le::model::load(bytes));
        check(false, message);
    } catch (const le::model::ArtifactError& error) {
        check(error.kind() == kind, message);
    }
}

} // namespace

int main() {
    const le::model::Artifact prefix{
        LE_ABI_VERSION, LE_MODEL_PREFIX, 7, {"en", "zh"}, {}, LE_PREFIX_FIXED, 2, 0.5F};
    const auto prefix_bytes = le::model::encode(prefix);
    check(prefix_bytes.size() == 84, "prefix fixture has stable encoded size");
    check(std::string_view(reinterpret_cast<const char*>(prefix_bytes.data()), 7) == "LAEMODL",
          "artifact starts with stable magic");
    check(read_u32(prefix_bytes, 12) == 64 && read_u32(prefix_bytes, 16) == 84,
          "artifact header and total sizes are encoded");
    check(read_u32(prefix_bytes, 20) == le::model::checksum(prefix_bytes),
          "artifact stores its CRC-32 checksum");
    check(read_u32(prefix_bytes, 20) == 0xE1304C0BU,
          "prefix artifact matches the v1 golden checksum");
    const auto loaded_prefix = le::model::load(prefix_bytes);
    check(loaded_prefix.type == LE_MODEL_PREFIX && loaded_prefix.model_version == 7,
          "prefix type and model version round trip");
    check(loaded_prefix.languages == std::vector<std::string>{"en", "zh"},
          "language metadata round trips");
    check(loaded_prefix.prefix_strategy == LE_PREFIX_FIXED && loaded_prefix.fixed_graphemes == 2,
          "prefix parameters round trip");
    check(le::model::supports_language(loaded_prefix, "en-US"),
          "primary language capability covers regional tag");
    check(!le::model::supports_language(loaded_prefix, "ja"),
          "language capability rejects unsupported tag");
    check(le::model::encode(loaded_prefix) == prefix_bytes,
          "artifact encoding is byte-for-byte deterministic");

    const le::model::Artifact lexical{
        LE_ABI_VERSION,
        LE_MODEL_LEXICAL_CORE,
        3,
        {"en", "zh"},
        {LE_FEATURE_LEXICAL_CORE},
        LE_PREFIX_PROPORTIONAL,
        1,
        0.5F,
    };
    const auto lexical_bytes = le::model::encode(lexical);
    check(lexical_bytes.size() == 76, "lexical fixture has stable encoded size");
    const auto loaded_lexical = le::model::load(lexical_bytes);
    check(loaded_lexical.required_features == std::vector<std::uint32_t>{LE_FEATURE_LEXICAL_CORE},
          "required feature metadata round trips");

    auto corrupted = prefix_bytes;
    corrupted.back() ^= 0x01U;
    expect_error(corrupted, le::model::ErrorKind::invalid, "checksum corruption is rejected");

    auto bad_magic = prefix_bytes;
    bad_magic.front() = 'X';
    reseal(bad_magic);
    expect_error(bad_magic, le::model::ErrorKind::invalid, "invalid magic is rejected");

    auto future_format = prefix_bytes;
    future_format[10] = 1;
    reseal(future_format);
    expect_error(future_format, le::model::ErrorKind::incompatible,
                 "future format minor is incompatible");

    auto future_abi = prefix_bytes;
    set_u32(future_abi, 24, LE_ABI_VERSION + 1);
    reseal(future_abi);
    expect_error(future_abi, le::model::ErrorKind::incompatible,
                 "future runtime ABI requirement is incompatible");

    auto unknown_feature = lexical_bytes;
    set_u32(unknown_feature, read_u32(unknown_feature, 48), 0x7FFFFFFFU);
    reseal(unknown_feature);
    expect_error(unknown_feature, le::model::ErrorKind::incompatible,
                 "unknown required feature is incompatible");

    auto bad_offset = prefix_bytes;
    set_u32(bad_offset, 48, 65);
    reseal(bad_offset);
    expect_error(bad_offset, le::model::ErrorKind::invalid,
                 "inconsistent table offset is rejected");

    try {
        auto duplicate_language = prefix;
        duplicate_language.languages.push_back("EN");
        static_cast<void>(le::model::encode(duplicate_language));
        check(false, "duplicate language metadata is rejected");
    } catch (const le::model::ArtifactError& error) {
        check(error.kind() == le::model::ErrorKind::invalid,
              "duplicate language metadata is rejected");
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All model artifact tests passed\n";
    return EXIT_SUCCESS;
}
