#include "le/api.h"

#include "core/pipeline.hpp"
#include "core/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct le_runtime {
    std::uint32_t abi_version = LE_ABI_VERSION;
};

struct le_result {
    std::vector<le_emphasis_t> emphasis;
};

struct le_analysis {
    std::vector<le_analysis_node_t> nodes;
    std::vector<le_node_id_t> children;
    std::vector<le_feature_t> features;
    std::vector<std::string> languages;
    std::vector<le_language_region_t> language_regions;
};

namespace {

thread_local std::array<char, 512> current_error{};
thread_local std::size_t current_error_size = 0;

void set_error(std::string_view message) noexcept {
    current_error_size = std::min(message.size(), current_error.size() - 1);
    std::memcpy(current_error.data(), message.data(), current_error_size);
    current_error[current_error_size] = '\0';
}

le_status_t invalid_argument(std::string_view message) noexcept {
    set_error(message);
    return LE_ERROR_INVALID_ARGUMENT;
}

bool valid_view(le_string_view_t view) { return view.data != nullptr || view.size == 0; }

bool valid_language(le_string_view_t language) noexcept {
    if (!valid_view(language) || language.size > 255) {
        return false;
    }
    if (language.size == 0) {
        return true;
    }
    bool previous_hyphen = true;
    for (std::size_t index = 0; index < language.size; ++index) {
        const auto character = static_cast<unsigned char>(language.data[index]);
        const bool hyphen = character == '-';
        const bool alpha_numeric = (character >= 'a' && character <= 'z') ||
                                   (character >= 'A' && character <= 'Z') ||
                                   (character >= '0' && character <= '9');
        if ((!hyphen && !alpha_numeric) || (hyphen && previous_hyphen)) {
            return false;
        }
        previous_hyphen = hyphen;
    }
    return !previous_hyphen;
}

std::string_view language_view(le_string_view_t language) {
    return language.size == 0 ? std::string_view("und")
                              : std::string_view(language.data, language.size);
}

le_node_kind_t to_abi(le::core::NodeKind kind) {
    switch (kind) {
    case le::core::NodeKind::document:
        return LE_NODE_DOCUMENT;
    case le::core::NodeKind::block:
        return LE_NODE_BLOCK;
    case le::core::NodeKind::paragraph:
        return LE_NODE_PARAGRAPH;
    case le::core::NodeKind::sentence:
        return LE_NODE_SENTENCE;
    case le::core::NodeKind::unit:
        return LE_NODE_UNIT;
    case le::core::NodeKind::subunit:
        return LE_NODE_SUBUNIT;
    }
    throw std::logic_error("unrecognized internal node kind");
}

std::unique_ptr<le_analysis> make_analysis(const le::core::Analysis& source) {
    auto result = std::make_unique<le_analysis>();
    if (source.nodes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("analysis node count exceeds v1 identifier capacity");
    }
    result->nodes.reserve(source.nodes.size());
    for (const auto& node : source.nodes) {
        constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
        if (result->children.size() > maximum ||
            node.children.size() > maximum - result->children.size() ||
            result->features.size() > maximum ||
            node.features.size() > maximum - result->features.size()) {
            throw std::length_error("analysis arrays exceed v1 index capacity");
        }
        const auto first_child = static_cast<std::uint32_t>(result->children.size());
        const auto first_feature = static_cast<std::uint32_t>(result->features.size());
        for (const auto child : node.children) {
            result->children.push_back(child.value());
        }
        for (const auto& feature : node.features) {
            result->features.push_back(le_feature_t{feature.id, feature.value});
        }
        result->nodes.push_back(
            le_analysis_node_t{node.id.value(), to_abi(node.kind),
                               le_text_span_t{node.span.begin().value(), node.span.end().value()},
                               first_child, static_cast<std::uint32_t>(node.children.size()),
                               first_feature, static_cast<std::uint32_t>(node.features.size())});
    }

    result->languages.reserve(source.language_regions.size());
    for (const auto& region : source.language_regions) {
        result->languages.push_back(region.language);
    }
    result->language_regions.reserve(source.language_regions.size());
    for (std::size_t index = 0; index < source.language_regions.size(); ++index) {
        const auto& source_region = source.language_regions[index];
        const auto& language = result->languages[index];
        result->language_regions.push_back(le_language_region_t{
            le_text_span_t{source_region.span.begin().value(), source_region.span.end().value()},
            le_string_view_t{language.data(), language.size()}, source_region.confidence, 0});
    }
    return result;
}

le::core::PipelineOptions defaults() {
    return le::core::PipelineOptions{
        "und", le::core::PrefixModelConfig{le::core::PrefixStrategy::proportional, 1, 0.5F}, 1.0F};
}

le_status_t read_options(const le_process_options_t* source, le::core::PipelineOptions& target) {
    target = defaults();
    if (source == nullptr) {
        return LE_OK;
    }
    if (source->struct_size < LE_PROCESS_OPTIONS_V1_SIZE) {
        return invalid_argument("process options struct_size is smaller than v1");
    }
    if (source->flags != 0) {
        return invalid_argument("process options contain unsupported flags");
    }
    if (!valid_language(source->language)) {
        return invalid_argument("language must be an empty or BCP-47-compatible ASCII tag");
    }
    if (!std::isfinite(source->prefix_proportion) || source->prefix_proportion < 0.0F ||
        source->prefix_proportion > 1.0F) {
        return invalid_argument("prefix_proportion must be finite and in [0, 1]");
    }
    if (!std::isfinite(source->emphasis_strength) || source->emphasis_strength < 0.0F ||
        source->emphasis_strength > 1.0F) {
        return invalid_argument("emphasis_strength must be finite and in [0, 1]");
    }
    if (source->prefix_strategy != LE_PREFIX_PROPORTIONAL &&
        source->prefix_strategy != LE_PREFIX_FIXED) {
        return invalid_argument("prefix_strategy is not recognized");
    }

    target.language = language_view(source->language);
    target.prefix = le::core::PrefixModelConfig{source->prefix_strategy == LE_PREFIX_FIXED
                                                    ? le::core::PrefixStrategy::fixed
                                                    : le::core::PrefixStrategy::proportional,
                                                source->fixed_graphemes, source->prefix_proportion};
    target.emphasis_strength = source->emphasis_strength;
    return LE_OK;
}

} // namespace

extern "C" {

void le_runtime_config_init(le_runtime_config_t* config) {
    if (config != nullptr) {
        *config = le_runtime_config_t{LE_RUNTIME_CONFIG_V1_SIZE, 0};
    }
}

void le_process_options_init(le_process_options_t* options) {
    if (options != nullptr) {
        *options = le_process_options_t{LE_PROCESS_OPTIONS_V1_SIZE,
                                        0,
                                        le_string_view_t{nullptr, 0},
                                        LE_PREFIX_PROPORTIONAL,
                                        1,
                                        0.5F,
                                        1.0F};
    }
}

le_status_t le_runtime_create(const le_runtime_config_t* config, le_runtime_t** out_runtime) {
    if (out_runtime == nullptr) {
        return invalid_argument("out_runtime is null");
    }
    *out_runtime = nullptr;
    if (config != nullptr) {
        if (config->struct_size < LE_RUNTIME_CONFIG_V1_SIZE) {
            return invalid_argument("runtime config struct_size is smaller than v1");
        }
        if (config->flags != 0) {
            return invalid_argument("runtime config contains unsupported flags");
        }
    }

    try {
        *out_runtime = new le_runtime{};
        return LE_OK;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate runtime");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        set_error("unexpected failure while creating runtime");
        return LE_ERROR_INTERNAL;
    }
}

void le_runtime_destroy(le_runtime_t* runtime) { delete runtime; }

le_status_t le_analyze(le_runtime_t* runtime, le_string_view_t text, le_string_view_t language,
                       le_analysis_t** out_analysis) {
    if (out_analysis == nullptr) {
        return invalid_argument("out_analysis is null");
    }
    *out_analysis = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (!valid_view(text)) {
        return invalid_argument("text data is null while text size is nonzero");
    }
    if (!valid_language(language)) {
        return invalid_argument("language must be an empty or BCP-47-compatible ASCII tag");
    }
    if (text.size > std::numeric_limits<std::uint64_t>::max()) {
        return invalid_argument("text is too large for 64-bit byte offsets");
    }

    try {
        const std::string_view bytes =
            text.size == 0 ? std::string_view{} : std::string_view(text.data, text.size);
        const le::core::Text core_text(bytes);
        const auto core_analysis = le::core::analyze(core_text, language_view(language));
        auto analysis = make_analysis(core_analysis);
        *out_analysis = analysis.release();
        return LE_OK;
    } catch (const le::core::InvalidUtf8& error) {
        set_error(error.what());
        return LE_ERROR_INVALID_UTF8;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate analysis result");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while analyzing text");
        return LE_ERROR_INTERNAL;
    }
}

size_t le_analysis_node_count(const le_analysis_t* analysis) {
    return analysis == nullptr ? 0 : analysis->nodes.size();
}

const le_analysis_node_t* le_analysis_node_data(const le_analysis_t* analysis) {
    return analysis == nullptr || analysis->nodes.empty() ? nullptr : analysis->nodes.data();
}

size_t le_analysis_child_count(const le_analysis_t* analysis) {
    return analysis == nullptr ? 0 : analysis->children.size();
}

const le_node_id_t* le_analysis_child_data(const le_analysis_t* analysis) {
    return analysis == nullptr || analysis->children.empty() ? nullptr : analysis->children.data();
}

size_t le_analysis_feature_count(const le_analysis_t* analysis) {
    return analysis == nullptr ? 0 : analysis->features.size();
}

const le_feature_t* le_analysis_feature_data(const le_analysis_t* analysis) {
    return analysis == nullptr || analysis->features.empty() ? nullptr : analysis->features.data();
}

size_t le_analysis_language_region_count(const le_analysis_t* analysis) {
    return analysis == nullptr ? 0 : analysis->language_regions.size();
}

const le_language_region_t* le_analysis_language_region_data(const le_analysis_t* analysis) {
    return analysis == nullptr || analysis->language_regions.empty()
               ? nullptr
               : analysis->language_regions.data();
}

void le_analysis_destroy(le_analysis_t* analysis) { delete analysis; }

le_status_t le_process(le_runtime_t* runtime, le_string_view_t text,
                       const le_process_options_t* options, le_result_t** out_result) {
    if (out_result == nullptr) {
        return invalid_argument("out_result is null");
    }
    *out_result = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (!valid_view(text)) {
        return invalid_argument("text data is null while text size is nonzero");
    }
    if (text.size > std::numeric_limits<std::uint64_t>::max()) {
        return invalid_argument("text is too large for 64-bit byte offsets");
    }

    try {
        le::core::PipelineOptions pipeline_options = defaults();
        const auto options_status = read_options(options, pipeline_options);
        if (options_status != LE_OK) {
            return options_status;
        }

        const std::string_view bytes =
            text.size == 0 ? std::string_view{} : std::string_view(text.data, text.size);
        const le::core::Text core_text(bytes);
        const auto core_emphasis = le::core::process(core_text, pipeline_options);

        auto result = std::make_unique<le_result>();
        result->emphasis.reserve(core_emphasis.size());
        for (const auto& item : core_emphasis) {
            result->emphasis.push_back(
                le_emphasis_t{le_text_span_t{item.span.begin().value(), item.span.end().value()},
                              item.strength, item.style_class});
        }
        *out_result = result.release();
        return LE_OK;
    } catch (const le::core::InvalidUtf8& error) {
        set_error(error.what());
        return LE_ERROR_INVALID_UTF8;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate processing result");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while processing text");
        return LE_ERROR_INTERNAL;
    }
}

le_string_view_t le_runtime_last_error(const le_runtime_t* runtime) {
    if (runtime == nullptr) {
        return le_string_view_t{nullptr, 0};
    }
    return le_string_view_t{current_error.data(), current_error_size};
}

const char* le_status_string(le_status_t status) {
    switch (status) {
    case LE_OK:
        return "LE_OK";
    case LE_ERROR_INVALID_ARGUMENT:
        return "LE_ERROR_INVALID_ARGUMENT";
    case LE_ERROR_INVALID_UTF8:
        return "LE_ERROR_INVALID_UTF8";
    case LE_ERROR_OUT_OF_MEMORY:
        return "LE_ERROR_OUT_OF_MEMORY";
    case LE_ERROR_UNSUPPORTED_LANGUAGE:
        return "LE_ERROR_UNSUPPORTED_LANGUAGE";
    case LE_ERROR_MODEL_INVALID:
        return "LE_ERROR_MODEL_INVALID";
    case LE_ERROR_MODEL_INCOMPATIBLE:
        return "LE_ERROR_MODEL_INCOMPATIBLE";
    case LE_ERROR_PLUGIN_FAILURE:
        return "LE_ERROR_PLUGIN_FAILURE";
    case LE_ERROR_INTERNAL:
        return "LE_ERROR_INTERNAL";
    default:
        return "LE_ERROR_UNKNOWN";
    }
}

size_t le_result_emphasis_count(const le_result_t* result) {
    return result == nullptr ? 0 : result->emphasis.size();
}

const le_emphasis_t* le_result_emphasis_data(const le_result_t* result) {
    return result == nullptr || result->emphasis.empty() ? nullptr : result->emphasis.data();
}

void le_result_destroy(le_result_t* result) { delete result; }

} // extern "C"
