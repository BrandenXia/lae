#include "model/artifact.hpp"

#include "le/analysis.h"
#include "le/model.h"
#include "le/reading.h"
#include "le/version.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace le::model {
namespace {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "model artifacts require IEEE-754 binary32 floats");

constexpr std::array<std::uint8_t, 8> magic{'L', 'A', 'E', 'M', 'O', 'D', 'L', 0};
constexpr std::size_t checksum_offset = 20;

void fail(ErrorKind kind, std::string_view message) {
    throw ArtifactError(kind, std::string(message));
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        fail(ErrorKind::invalid, "model artifact contains a truncated 16-bit field");
    }
    return static_cast<std::uint16_t>(bytes[offset]) | static_cast<std::uint16_t>(bytes[offset + 1])
                                                           << 8U;
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        fail(ErrorKind::invalid, "model artifact contains a truncated 32-bit field");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 8U |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16U |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

void write_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void set_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (8U * index));
    }
}

bool valid_language(std::string_view language) {
    if (language.empty() || language.size() > 255) {
        return false;
    }
    bool previous_hyphen = true;
    for (const auto character : language) {
        const auto byte = static_cast<unsigned char>(character);
        const auto hyphen = byte == '-';
        const auto alpha_numeric = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                                   (byte >= '0' && byte <= '9');
        if ((!hyphen && !alpha_numeric) || (hyphen && previous_hyphen)) {
            return false;
        }
        previous_hyphen = hyphen;
    }
    return !previous_hyphen;
}

bool ascii_equal(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::ranges::equal(left, right, [](char left_character, char right_character) {
               const auto lower = [](char character) {
                   return character >= 'A' && character <= 'Z'
                              ? static_cast<char>(character + ('a' - 'A'))
                              : character;
               };
               return lower(left_character) == lower(right_character);
           });
}

bool language_matches(std::string_view supported, std::string_view requested) {
    if (ascii_equal(supported, requested)) {
        return true;
    }
    return requested.size() > supported.size() && requested[supported.size()] == '-' &&
           ascii_equal(supported, requested.substr(0, supported.size()));
}

bool known_feature(std::uint32_t feature) {
    constexpr std::array known{
        std::uint32_t(LE_FEATURE_BOUNDARY_STRENGTH),
        std::uint32_t(LE_FEATURE_GRAPHEME_COUNT),
        std::uint32_t(LE_FEATURE_SEGMENTATION_CONFIDENCE),
        std::uint32_t(LE_FEATURE_LEXICAL_CORE),
        std::uint32_t(LE_FEATURE_DERIVATIONAL_AFFIX),
        std::uint32_t(LE_FEATURE_GRAMMATICAL_AFFIX),
        std::uint32_t(LE_FEATURE_CONTENT_UNIT),
        std::uint32_t(LE_FEATURE_SCRIPT_HAN),
        std::uint32_t(LE_FEATURE_SCRIPT_LATIN),
    };
    return std::ranges::find(known, feature) != known.end();
}

std::uint32_t float_bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(float));
    return bits;
}

float bits_float(std::uint32_t bits) {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

void validate_metadata(const Artifact& artifact) {
    if (artifact.minimum_abi_version > LE_ABI_VERSION ||
        (artifact.minimum_abi_version >> 16U) != LE_ABI_VERSION_MAJOR) {
        fail(ErrorKind::incompatible, "model requires an incompatible runtime ABI");
    }
    if (artifact.model_version == 0) {
        fail(ErrorKind::invalid, "model version must be nonzero");
    }
    if (artifact.languages.size() > 64 || artifact.required_features.size() > 256) {
        fail(ErrorKind::invalid, "model metadata count exceeds the format limit");
    }
    std::vector<std::string_view> languages;
    for (const auto& language : artifact.languages) {
        if (!valid_language(language)) {
            fail(ErrorKind::invalid, "model contains an invalid language tag");
        }
        if (std::ranges::any_of(languages, [&](std::string_view existing) {
                return ascii_equal(existing, language);
            })) {
            fail(ErrorKind::invalid, "model contains a duplicate language tag");
        }
        languages.push_back(language);
    }
    std::vector<std::uint32_t> features;
    for (const auto feature : artifact.required_features) {
        if (!known_feature(feature)) {
            fail(ErrorKind::incompatible, "model requires an unsupported feature identifier");
        }
        if (std::ranges::find(features, feature) != features.end()) {
            fail(ErrorKind::invalid, "model contains a duplicate required feature");
        }
        features.push_back(feature);
    }
    if (artifact.type == LE_MODEL_PREFIX) {
        if (artifact.prefix_strategy != LE_PREFIX_PROPORTIONAL &&
            artifact.prefix_strategy != LE_PREFIX_FIXED) {
            fail(ErrorKind::invalid, "prefix model contains an invalid strategy");
        }
        if (!std::isfinite(artifact.prefix_proportion) || artifact.prefix_proportion < 0.0F ||
            artifact.prefix_proportion > 1.0F) {
            fail(ErrorKind::invalid, "prefix model proportion is not in [0, 1]");
        }
    } else if (artifact.type == LE_MODEL_LEXICAL_CORE) {
        if (std::ranges::find(artifact.required_features, std::uint32_t(LE_FEATURE_LEXICAL_CORE)) ==
            artifact.required_features.end()) {
            fail(ErrorKind::invalid, "lexical-core model does not declare its required feature");
        }
    } else if (artifact.type == LE_MODEL_LINEAR_SALIENCE) {
        constexpr auto minimum_linear_abi = (1U << 16U) | 6U;
        if (artifact.minimum_abi_version < minimum_linear_abi) {
            fail(ErrorKind::invalid, "linear salience model minimum ABI predates model support");
        }
        if (!std::isfinite(artifact.linear_bias)) {
            fail(ErrorKind::invalid, "linear salience model bias is not finite");
        }
        if (artifact.linear_weights.empty() || artifact.linear_weights.size() > 256) {
            fail(ErrorKind::invalid, "linear salience model weight count is invalid");
        }
        std::vector<std::uint32_t> weighted_features;
        for (const auto& item : artifact.linear_weights) {
            if (!known_feature(item.feature)) {
                fail(ErrorKind::incompatible,
                     "linear salience model uses an unsupported feature identifier");
            }
            if (!std::isfinite(item.weight)) {
                fail(ErrorKind::invalid, "linear salience model contains a non-finite weight");
            }
            if (std::ranges::find(weighted_features, item.feature) != weighted_features.end()) {
                fail(ErrorKind::invalid, "linear salience model contains a duplicate weight");
            }
            if (std::ranges::find(artifact.required_features, item.feature) ==
                artifact.required_features.end()) {
                fail(ErrorKind::invalid,
                     "linear salience model weight is absent from required features");
            }
            weighted_features.push_back(item.feature);
        }
    } else {
        fail(ErrorKind::incompatible, "model type is not supported by this runtime");
    }
}

} // namespace

std::uint32_t checksum(std::span<const std::uint8_t> bytes) {
    std::uint32_t value = 0xFFFFFFFFU;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto byte =
            index >= checksum_offset && index < checksum_offset + 4 ? 0 : bytes[index];
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value >> 1U) ^ (0xEDB88320U & (0U - (value & 1U)));
        }
    }
    return ~value;
}

Artifact load(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < header_size || bytes.size() > maximum_artifact_size) {
        fail(ErrorKind::invalid, "model artifact size is outside the supported range");
    }
    if (!std::ranges::equal(magic, bytes.first(magic.size()))) {
        fail(ErrorKind::invalid, "model artifact magic is invalid");
    }
    const auto format_major = read_u16(bytes, 8);
    const auto format_minor = read_u16(bytes, 10);
    if (format_major != LE_MODEL_FORMAT_VERSION_MAJOR ||
        format_minor > LE_MODEL_FORMAT_VERSION_MINOR) {
        fail(ErrorKind::incompatible, "model artifact format version is not supported");
    }
    if (read_u32(bytes, 12) != header_size || read_u32(bytes, 16) != bytes.size()) {
        fail(ErrorKind::invalid, "model artifact size metadata is inconsistent");
    }
    if (read_u32(bytes, checksum_offset) != checksum(bytes)) {
        fail(ErrorKind::invalid, "model artifact checksum does not match its contents");
    }
    if (read_u32(bytes, 60) != 0) {
        fail(ErrorKind::incompatible, "model artifact uses unsupported flags");
    }

    const auto language_count = read_u32(bytes, 36);
    const auto feature_count = read_u32(bytes, 40);
    const auto languages_offset = read_u32(bytes, 44);
    const auto features_offset = read_u32(bytes, 48);
    const auto parameters_offset = read_u32(bytes, 52);
    const auto parameter_count = read_u32(bytes, 56);
    if (language_count > 64 || feature_count > 256 || languages_offset != header_size) {
        fail(ErrorKind::invalid, "model artifact metadata table is invalid");
    }

    Artifact artifact{read_u32(bytes, 24),
                      read_u32(bytes, 28),
                      read_u32(bytes, 32),
                      {},
                      {},
                      LE_PREFIX_PROPORTIONAL,
                      1,
                      0.5F,
                      0.0F,
                      {}};
    std::size_t cursor = languages_offset;
    artifact.languages.reserve(language_count);
    for (std::uint32_t index = 0; index < language_count; ++index) {
        const auto length = read_u16(bytes, cursor);
        cursor += 2;
        if (cursor > bytes.size() || bytes.size() - cursor < length) {
            fail(ErrorKind::invalid, "model language table is truncated");
        }
        artifact.languages.emplace_back(reinterpret_cast<const char*>(bytes.data() + cursor),
                                        length);
        cursor += length;
    }
    if (cursor != features_offset || feature_count > (bytes.size() - cursor) / 4) {
        fail(ErrorKind::invalid, "model feature table offset is invalid");
    }
    artifact.required_features.reserve(feature_count);
    for (std::uint32_t index = 0; index < feature_count; ++index) {
        artifact.required_features.push_back(read_u32(bytes, cursor));
        cursor += 4;
    }
    if (cursor != parameters_offset || parameter_count > (bytes.size() - cursor) / 4 ||
        cursor + static_cast<std::size_t>(parameter_count) * 4 != bytes.size()) {
        fail(ErrorKind::invalid, "model parameter table offset is invalid");
    }
    if (artifact.type == LE_MODEL_PREFIX) {
        if (parameter_count != 3) {
            fail(ErrorKind::invalid, "prefix model parameter count is invalid");
        }
        artifact.prefix_strategy = read_u32(bytes, cursor);
        artifact.fixed_graphemes = read_u32(bytes, cursor + 4);
        artifact.prefix_proportion = bits_float(read_u32(bytes, cursor + 8));
    } else if (artifact.type == LE_MODEL_LEXICAL_CORE && parameter_count != 0) {
        fail(ErrorKind::invalid, "lexical-core model must not contain parameters");
    } else if (artifact.type == LE_MODEL_LINEAR_SALIENCE) {
        if (parameter_count < 4 || parameter_count % 2 != 0) {
            fail(ErrorKind::invalid, "linear salience model parameter count is invalid");
        }
        artifact.linear_bias = bits_float(read_u32(bytes, cursor));
        const auto weight_count = read_u32(bytes, cursor + 4);
        if (weight_count == 0 || weight_count > 256 || parameter_count != 2 + 2 * weight_count) {
            fail(ErrorKind::invalid, "linear salience model weight table is invalid");
        }
        artifact.linear_weights.reserve(weight_count);
        cursor += 8;
        for (std::uint32_t index = 0; index < weight_count; ++index) {
            artifact.linear_weights.push_back(Artifact::FeatureWeight{
                read_u32(bytes, cursor), bits_float(read_u32(bytes, cursor + 4))});
            cursor += 8;
        }
    }
    validate_metadata(artifact);
    return artifact;
}

std::vector<std::uint8_t> encode(const Artifact& artifact) {
    validate_metadata(artifact);
    const auto parameter_count =
        artifact.type == LE_MODEL_PREFIX ? 3U
        : artifact.type == LE_MODEL_LINEAR_SALIENCE
            ? static_cast<std::uint32_t>(2 + 2 * artifact.linear_weights.size())
            : 0U;
    std::vector<std::uint8_t> bytes(magic.begin(), magic.end());
    write_u16(bytes, LE_MODEL_FORMAT_VERSION_MAJOR);
    write_u16(bytes, LE_MODEL_FORMAT_VERSION_MINOR);
    write_u32(bytes, header_size);
    write_u32(bytes, 0);
    write_u32(bytes, 0);
    write_u32(bytes, artifact.minimum_abi_version);
    write_u32(bytes, artifact.type);
    write_u32(bytes, artifact.model_version);
    write_u32(bytes, static_cast<std::uint32_t>(artifact.languages.size()));
    write_u32(bytes, static_cast<std::uint32_t>(artifact.required_features.size()));
    write_u32(bytes, header_size);
    const auto features_offset_index = bytes.size();
    write_u32(bytes, 0);
    const auto parameters_offset_index = bytes.size();
    write_u32(bytes, 0);
    write_u32(bytes, parameter_count);
    write_u32(bytes, 0);

    for (const auto& language : artifact.languages) {
        write_u16(bytes, static_cast<std::uint16_t>(language.size()));
        bytes.insert(bytes.end(), language.begin(), language.end());
    }
    set_u32(bytes, features_offset_index, static_cast<std::uint32_t>(bytes.size()));
    for (const auto feature : artifact.required_features) {
        write_u32(bytes, feature);
    }
    set_u32(bytes, parameters_offset_index, static_cast<std::uint32_t>(bytes.size()));
    if (artifact.type == LE_MODEL_PREFIX) {
        write_u32(bytes, artifact.prefix_strategy);
        write_u32(bytes, artifact.fixed_graphemes);
        write_u32(bytes, float_bits(artifact.prefix_proportion));
    } else if (artifact.type == LE_MODEL_LINEAR_SALIENCE) {
        write_u32(bytes, float_bits(artifact.linear_bias));
        write_u32(bytes, static_cast<std::uint32_t>(artifact.linear_weights.size()));
        for (const auto& item : artifact.linear_weights) {
            write_u32(bytes, item.feature);
            write_u32(bytes, float_bits(item.weight));
        }
    }
    if (bytes.size() > maximum_artifact_size ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
        fail(ErrorKind::invalid, "encoded model exceeds the format size limit");
    }
    set_u32(bytes, 16, static_cast<std::uint32_t>(bytes.size()));
    set_u32(bytes, checksum_offset, checksum(bytes));
    return bytes;
}

bool supports_language(const Artifact& artifact, std::string_view language) {
    return artifact.languages.empty() ||
           std::ranges::any_of(artifact.languages, [&](const std::string& supported) {
               return language_matches(supported, language);
           });
}

} // namespace le::model
