#include "le/api.h"

#include "core/pipeline.hpp"
#include "core/text.hpp"
#include "model/artifact.hpp"
#include "plugin/provider_registry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct le_runtime {
    std::uint32_t abi_version = LE_ABI_VERSION;
    le::plugin::ProviderRegistry providers;
};

struct le_result {
    std::vector<le_emphasis_t> emphasis;
};

struct le_analysis {
    std::string text;
    le::core::Analysis core;
    std::vector<le_analysis_node_t> nodes;
    std::vector<le_node_id_t> children;
    std::vector<le_feature_t> features;
    std::vector<le_language_region_t> language_regions;
};

struct le_signal_result {
    std::vector<le::core::ReadingSignal> core;
    std::vector<le_reading_signal_t> signals;
};

struct le_model {
    le::model::Artifact core;
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

std::unique_ptr<le_analysis> make_analysis(le::core::Analysis source, std::string_view text) {
    auto result = std::make_unique<le_analysis>();
    result->text = text;
    result->core = std::move(source);
    const auto& stable_source = result->core;
    if (stable_source.nodes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("analysis node count exceeds v1 identifier capacity");
    }
    result->nodes.reserve(stable_source.nodes.size());
    for (const auto& node : stable_source.nodes) {
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

    result->language_regions.reserve(stable_source.language_regions.size());
    for (std::size_t index = 0; index < stable_source.language_regions.size(); ++index) {
        const auto& source_region = stable_source.language_regions[index];
        const auto& language = source_region.language;
        result->language_regions.push_back(le_language_region_t{
            le_text_span_t{source_region.span.begin().value(), source_region.span.end().value()},
            le_string_view_t{language.data(), language.size()}, source_region.confidence, 0});
    }
    return result;
}

std::unique_ptr<le_signal_result> make_signals(std::vector<le::core::ReadingSignal> source) {
    auto result = std::make_unique<le_signal_result>();
    result->core = std::move(source);
    result->signals.reserve(result->core.size());
    for (const auto& signal : result->core) {
        result->signals.push_back(le_reading_signal_t{
            le_text_span_t{signal.span.begin().value(), signal.span.end().value()},
            signal.fixation_salience, signal.lexical_salience, signal.reading_difficulty, 0});
    }
    return result;
}

std::unique_ptr<le_result> make_result(const std::vector<le::core::Emphasis>& source) {
    auto result = std::make_unique<le_result>();
    result->emphasis.reserve(source.size());
    for (const auto& item : source) {
        result->emphasis.push_back(
            le_emphasis_t{le_text_span_t{item.span.begin().value(), item.span.end().value()},
                          item.strength, item.style_class});
    }
    return result;
}

le::core::PrefixModelConfig prefix_defaults() {
    return le::core::PrefixModelConfig{le::core::PrefixStrategy::proportional, 1, 0.5F};
}

le::core::PresentationConfig presentation_defaults() {
    return le::core::PresentationConfig{le::core::PresentationPolicy::binary, 0.0F, 0.0F, 1.0F};
}

le::core::PipelineOptions defaults() {
    return le::core::PipelineOptions{"und", prefix_defaults(), presentation_defaults(),
                                     le::core::ReadingModelKind::prefix};
}

le::core::Analysis analyze_with_runtime(le_runtime& runtime, const le::core::Text& text,
                                        std::string_view language) {
    if (auto analysis = runtime.providers.analyze(text, language)) {
        return std::move(*analysis);
    }
    return le::core::analyze(text, language);
}

bool model_supports_analysis_languages(const le::model::Artifact& model,
                                       const le::core::Analysis& analysis) {
    return std::ranges::all_of(analysis.language_regions,
                               [&](const le::core::LanguageRegion& region) {
                                   return le::model::supports_language(model, region.language);
                               });
}

std::vector<le::core::ReadingSignal> generate_model_signals(const le::core::Text& text,
                                                            const le::core::Analysis& analysis,
                                                            const le::model::Artifact& model) {
    if (model.type == LE_MODEL_PREFIX) {
        const auto strategy = model.prefix_strategy == LE_PREFIX_FIXED
                                  ? le::core::PrefixStrategy::fixed
                                  : le::core::PrefixStrategy::proportional;
        return le::core::PrefixReadingModel(le::core::PrefixModelConfig{strategy,
                                                                        model.fixed_graphemes,
                                                                        model.prefix_proportion})
            .generate(text, analysis);
    }
    if (model.type == LE_MODEL_LEXICAL_CORE) {
        return le::core::LexicalCoreReadingModel().generate(analysis);
    }
    if (model.type == LE_MODEL_LINEAR_SALIENCE) {
        std::vector<le::core::ReadingSignal> signals;
        for (const auto& node : analysis.nodes) {
            if (node.kind != le::core::NodeKind::unit) {
                continue;
            }
            auto salience = static_cast<double>(model.linear_bias);
            for (const auto& learned_weight : model.linear_weights) {
                const auto feature =
                    std::ranges::find_if(node.features, [&](const le::core::Feature& item) {
                        return item.id == learned_weight.feature;
                    });
                if (feature != node.features.end()) {
                    salience += static_cast<double>(learned_weight.weight) *
                                static_cast<double>(feature->value);
                }
            }
            const auto normalized = static_cast<float>(std::clamp(salience, 0.0, 1.0));
            if (normalized > 0.0F) {
                signals.push_back(le::core::ReadingSignal{node.span, normalized, normalized, 0.0F});
            }
        }
        return signals;
    }
    throw le::model::ArtifactError(le::model::ErrorKind::incompatible,
                                   "loaded model type is no longer supported");
}

le_status_t read_prefix_config(const le_prefix_model_config_t* source,
                               le::core::PrefixModelConfig& target) {
    target = prefix_defaults();
    if (source == nullptr) {
        return LE_OK;
    }
    if (source->struct_size < LE_PREFIX_MODEL_CONFIG_V1_SIZE) {
        return invalid_argument("prefix model config struct_size is smaller than v1");
    }
    if (source->flags != 0 || source->reserved != 0) {
        return invalid_argument("prefix model config contains unsupported fields");
    }
    if (source->strategy != LE_PREFIX_PROPORTIONAL && source->strategy != LE_PREFIX_FIXED) {
        return invalid_argument("prefix model strategy is not recognized");
    }
    if (!std::isfinite(source->proportion) || source->proportion < 0.0F ||
        source->proportion > 1.0F) {
        return invalid_argument("prefix model proportion must be finite and in [0, 1]");
    }
    target = le::core::PrefixModelConfig{source->strategy == LE_PREFIX_FIXED
                                             ? le::core::PrefixStrategy::fixed
                                             : le::core::PrefixStrategy::proportional,
                                         source->fixed_graphemes, source->proportion};
    return LE_OK;
}

le_status_t validate_presentation(le_presentation_policy_t policy, float threshold,
                                  float minimum_strength, float maximum_strength,
                                  le::core::PresentationConfig& target) {
    if (policy != LE_POLICY_BINARY && policy != LE_POLICY_VARIABLE_STRENGTH) {
        return invalid_argument("presentation policy is not recognized");
    }
    if (!std::isfinite(threshold) || threshold < 0.0F || threshold > 1.0F) {
        return invalid_argument("salience threshold must be finite and in [0, 1]");
    }
    if (!std::isfinite(minimum_strength) || !std::isfinite(maximum_strength) ||
        minimum_strength < 0.0F || maximum_strength > 1.0F || minimum_strength > maximum_strength) {
        return invalid_argument("presentation strengths must satisfy 0 <= minimum <= maximum <= 1");
    }
    target = le::core::PresentationConfig{policy == LE_POLICY_VARIABLE_STRENGTH
                                              ? le::core::PresentationPolicy::variable_strength
                                              : le::core::PresentationPolicy::binary,
                                          threshold, minimum_strength, maximum_strength};
    return LE_OK;
}

le_status_t read_presentation_config(const le_presentation_config_t* source,
                                     le::core::PresentationConfig& target) {
    target = presentation_defaults();
    if (source == nullptr) {
        return LE_OK;
    }
    if (source->struct_size < LE_PRESENTATION_CONFIG_V1_SIZE) {
        return invalid_argument("presentation config struct_size is smaller than v1");
    }
    if (source->flags != 0) {
        return invalid_argument("presentation config contains unsupported flags");
    }
    return validate_presentation(source->policy, source->salience_threshold,
                                 source->minimum_strength, source->maximum_strength, target);
}

le_status_t read_options(const le_process_options_t* source, le::core::PipelineOptions& target) {
    target = defaults();
    if (source == nullptr) {
        return LE_OK;
    }
    if (source->struct_size < LE_PROCESS_OPTIONS_V1_SIZE) {
        return invalid_argument("process options struct_size is smaller than v1");
    }
    if (source->struct_size > LE_PROCESS_OPTIONS_V1_SIZE &&
        source->struct_size < LE_PROCESS_OPTIONS_V2_SIZE) {
        return invalid_argument("process options struct_size is between known ABI versions");
    }
    if (source->struct_size > LE_PROCESS_OPTIONS_V2_SIZE &&
        source->struct_size < LE_PROCESS_OPTIONS_V3_SIZE) {
        return invalid_argument("process options struct_size is between known ABI versions");
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
    target.presentation = presentation_defaults();
    target.presentation.maximum_strength = source->emphasis_strength;
    if (source->struct_size >= LE_PROCESS_OPTIONS_V2_SIZE) {
        const auto status = validate_presentation(
            source->presentation_policy, source->salience_threshold,
            source->minimum_emphasis_strength, source->emphasis_strength, target.presentation);
        if (status != LE_OK) {
            return status;
        }
    }
    if (source->struct_size >= LE_PROCESS_OPTIONS_V3_SIZE) {
#if UINTPTR_MAX > UINT32_MAX
        if (source->reserved_v2 != 0) {
            return invalid_argument("process options contain unsupported reserved fields");
        }
#endif
        if (source->reserved_v3 != 0) {
            return invalid_argument("process options contain unsupported reserved fields");
        }
        if (source->reading_model != LE_READING_MODEL_PREFIX &&
            source->reading_model != LE_READING_MODEL_LEXICAL_CORE) {
            return invalid_argument("reading model is not recognized");
        }
        target.reading_model = source->reading_model == LE_READING_MODEL_LEXICAL_CORE
                                   ? le::core::ReadingModelKind::lexical_core
                                   : le::core::ReadingModelKind::prefix;
    }
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
        *options = le_process_options_t{};
        options->struct_size = LE_PROCESS_OPTIONS_V3_SIZE;
        options->prefix_strategy = LE_PREFIX_PROPORTIONAL;
        options->fixed_graphemes = 1;
        options->prefix_proportion = 0.5F;
        options->emphasis_strength = 1.0F;
        options->presentation_policy = LE_POLICY_BINARY;
        options->reading_model = LE_READING_MODEL_PREFIX;
    }
}

void le_prefix_model_config_init(le_prefix_model_config_t* config) {
    if (config != nullptr) {
        *config = le_prefix_model_config_t{
            LE_PREFIX_MODEL_CONFIG_V1_SIZE, 0, LE_PREFIX_PROPORTIONAL, 1, 0.5F, 0};
    }
}

void le_presentation_config_init(le_presentation_config_t* config) {
    if (config != nullptr) {
        *config = le_presentation_config_t{
            LE_PRESENTATION_CONFIG_V1_SIZE, 0, LE_POLICY_BINARY, 0.0F, 0.0F, 1.0F};
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

le_status_t le_runtime_register_provider(le_runtime_t* runtime, const le_provider_v1_t* provider) {
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (provider == nullptr) {
        return invalid_argument("provider is null");
    }
    try {
        runtime->providers.register_provider(*provider);
        return LE_OK;
    } catch (const le::plugin::Error& error) {
        set_error(error.what());
        return error.status();
    } catch (const std::bad_alloc&) {
        set_error("could not register provider");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while registering provider");
        return LE_ERROR_INTERNAL;
    }
}

le_status_t le_runtime_load_provider(le_runtime_t* runtime, le_string_view_t path) {
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (!valid_view(path)) {
        return invalid_argument("provider path data is null while path size is nonzero");
    }
    try {
        const auto module_path =
            path.size == 0 ? std::string_view{} : std::string_view(path.data, path.size);
        runtime->providers.load(module_path);
        return LE_OK;
    } catch (const le::plugin::Error& error) {
        set_error(error.what());
        return error.status();
    } catch (const std::bad_alloc&) {
        set_error("could not load provider");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while loading provider");
        return LE_ERROR_INTERNAL;
    }
}

int le_runtime_dynamic_providers_enabled(void) {
    return le::plugin::dynamic_loading_enabled() ? 1 : 0;
}

size_t le_runtime_provider_count(const le_runtime_t* runtime) {
    return runtime == nullptr ? 0 : runtime->providers.size();
}

le_string_view_t le_runtime_provider_name_at(const le_runtime_t* runtime, size_t index) {
    if (runtime == nullptr) {
        return le_string_view_t{nullptr, 0};
    }
    const auto name = runtime->providers.name_at(index);
    return name.empty() ? le_string_view_t{nullptr, 0} : le_string_view_t{name.data(), name.size()};
}

le_status_t le_model_load(le_runtime_t* runtime, const void* data, size_t size,
                          le_model_t** out_model) {
    if (out_model == nullptr) {
        return invalid_argument("out_model is null");
    }
    *out_model = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (data == nullptr) {
        return invalid_argument("model data is null");
    }
    try {
        const auto bytes =
            std::span<const std::uint8_t>(static_cast<const std::uint8_t*>(data), size);
        auto model = std::make_unique<le_model>();
        model->core = le::model::load(bytes);
        *out_model = model.release();
        return LE_OK;
    } catch (const le::model::ArtifactError& error) {
        set_error(error.what());
        return error.kind() == le::model::ErrorKind::incompatible ? LE_ERROR_MODEL_INCOMPATIBLE
                                                                  : LE_ERROR_MODEL_INVALID;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate model");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while loading model");
        return LE_ERROR_INTERNAL;
    }
}

void le_model_destroy(le_model_t* model) { delete model; }

le_model_type_t le_model_type(const le_model_t* model) {
    return model == nullptr ? 0 : model->core.type;
}

uint32_t le_model_version(const le_model_t* model) {
    return model == nullptr ? 0 : model->core.model_version;
}

uint32_t le_model_minimum_abi_version(const le_model_t* model) {
    return model == nullptr ? 0 : model->core.minimum_abi_version;
}

size_t le_model_language_count(const le_model_t* model) {
    return model == nullptr ? 0 : model->core.languages.size();
}

le_string_view_t le_model_language_at(const le_model_t* model, size_t index) {
    if (model == nullptr || index >= model->core.languages.size()) {
        return le_string_view_t{nullptr, 0};
    }
    const auto& language = model->core.languages[index];
    return le_string_view_t{language.data(), language.size()};
}

int le_model_supports_language(const le_model_t* model, le_string_view_t language) {
    if (model == nullptr || !valid_language(language)) {
        return 0;
    }
    return le::model::supports_language(model->core, language_view(language)) ? 1 : 0;
}

size_t le_model_required_feature_count(const le_model_t* model) {
    return model == nullptr ? 0 : model->core.required_features.size();
}

const le_feature_id_t* le_model_required_feature_data(const le_model_t* model) {
    return model == nullptr || model->core.required_features.empty()
               ? nullptr
               : model->core.required_features.data();
}

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
        auto core_analysis = analyze_with_runtime(*runtime, core_text, language_view(language));
        auto analysis = make_analysis(std::move(core_analysis), bytes);
        *out_analysis = analysis.release();
        return LE_OK;
    } catch (const le::plugin::Error& error) {
        set_error(error.what());
        return error.status();
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

le_string_view_t le_analysis_text(const le_analysis_t* analysis) {
    return analysis == nullptr ? le_string_view_t{nullptr, 0}
                               : le_string_view_t{analysis->text.data(), analysis->text.size()};
}

void le_analysis_destroy(le_analysis_t* analysis) { delete analysis; }

le_status_t le_generate_prefix_signals(le_runtime_t* runtime, const le_analysis_t* analysis,
                                       const le_prefix_model_config_t* config,
                                       le_signal_result_t** out_signals) {
    if (out_signals == nullptr) {
        return invalid_argument("out_signals is null");
    }
    *out_signals = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (analysis == nullptr) {
        return invalid_argument("analysis is null");
    }
    try {
        le::core::PrefixModelConfig model_config = prefix_defaults();
        const auto config_status = read_prefix_config(config, model_config);
        if (config_status != LE_OK) {
            return config_status;
        }
        const std::string_view bytes(analysis->text);
        const le::core::Text core_text(bytes);
        le::core::validate_analysis(core_text, analysis->core);
        const le::core::PrefixReadingModel model(model_config);
        auto signals = make_signals(model.generate(core_text, analysis->core));
        *out_signals = signals.release();
        return LE_OK;
    } catch (const le::core::InvalidUtf8& error) {
        set_error(error.what());
        return LE_ERROR_INVALID_UTF8;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate reading signals");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while generating reading signals");
        return LE_ERROR_INTERNAL;
    }
}

le_status_t le_generate_lexical_core_signals(le_runtime_t* runtime, const le_analysis_t* analysis,
                                             le_signal_result_t** out_signals) {
    if (out_signals == nullptr) {
        return invalid_argument("out_signals is null");
    }
    *out_signals = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (analysis == nullptr) {
        return invalid_argument("analysis is null");
    }
    try {
        const std::string_view bytes(analysis->text);
        const le::core::Text core_text(bytes);
        le::core::validate_analysis(core_text, analysis->core);
        auto signals = make_signals(le::core::LexicalCoreReadingModel().generate(analysis->core));
        *out_signals = signals.release();
        return LE_OK;
    } catch (const le::core::InvalidUtf8& error) {
        set_error(error.what());
        return LE_ERROR_INVALID_UTF8;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate lexical-core reading signals");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while generating lexical-core reading signals");
        return LE_ERROR_INTERNAL;
    }
}

le_status_t le_generate_model_signals(le_runtime_t* runtime, const le_analysis_t* analysis,
                                      const le_model_t* model, le_signal_result_t** out_signals) {
    if (out_signals == nullptr) {
        return invalid_argument("out_signals is null");
    }
    *out_signals = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (analysis == nullptr) {
        return invalid_argument("analysis is null");
    }
    if (model == nullptr) {
        return invalid_argument("model is null");
    }
    try {
        const std::string_view bytes(analysis->text);
        const le::core::Text core_text(bytes);
        le::core::validate_analysis(core_text, analysis->core);
        if (!model_supports_analysis_languages(model->core, analysis->core)) {
            set_error("model does not support an analysis language");
            return LE_ERROR_UNSUPPORTED_LANGUAGE;
        }
        auto signals = make_signals(generate_model_signals(core_text, analysis->core, model->core));
        *out_signals = signals.release();
        return LE_OK;
    } catch (const le::model::ArtifactError& error) {
        set_error(error.what());
        return LE_ERROR_MODEL_INCOMPATIBLE;
    } catch (const le::core::InvalidUtf8& error) {
        set_error(error.what());
        return LE_ERROR_INVALID_UTF8;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate model reading signals");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while generating model reading signals");
        return LE_ERROR_INTERNAL;
    }
}

size_t le_signal_result_count(const le_signal_result_t* signals) {
    return signals == nullptr ? 0 : signals->signals.size();
}

const le_reading_signal_t* le_signal_result_data(const le_signal_result_t* signals) {
    return signals == nullptr || signals->signals.empty() ? nullptr : signals->signals.data();
}

void le_signal_result_destroy(le_signal_result_t* signals) { delete signals; }

le_status_t le_generate_emphasis(le_runtime_t* runtime, const le_signal_result_t* signals,
                                 const le_presentation_config_t* config, le_result_t** out_result) {
    if (out_result == nullptr) {
        return invalid_argument("out_result is null");
    }
    *out_result = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (signals == nullptr) {
        return invalid_argument("signals is null");
    }

    try {
        le::core::PresentationConfig presentation_config = presentation_defaults();
        const auto config_status = read_presentation_config(config, presentation_config);
        if (config_status != LE_OK) {
            return config_status;
        }
        const auto emphasis = le::core::generate_emphasis(signals->core, presentation_config);
        auto result = make_result(emphasis);
        *out_result = result.release();
        return LE_OK;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate emphasis result");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while generating emphasis");
        return LE_ERROR_INTERNAL;
    }
}

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
        const auto analysis = analyze_with_runtime(*runtime, core_text, pipeline_options.language);
        const auto signals =
            pipeline_options.reading_model == le::core::ReadingModelKind::lexical_core
                ? le::core::LexicalCoreReadingModel().generate(analysis)
                : le::core::PrefixReadingModel(pipeline_options.prefix)
                      .generate(core_text, analysis);
        const auto core_emphasis =
            le::core::generate_emphasis(signals, pipeline_options.presentation);

        auto result = make_result(core_emphasis);
        *out_result = result.release();
        return LE_OK;
    } catch (const le::plugin::Error& error) {
        set_error(error.what());
        return error.status();
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

le_status_t le_process_with_model(le_runtime_t* runtime, const le_model_t* model,
                                  le_string_view_t text, const le_process_options_t* options,
                                  le_result_t** out_result) {
    if (out_result == nullptr) {
        return invalid_argument("out_result is null");
    }
    *out_result = nullptr;
    if (runtime == nullptr) {
        return invalid_argument("runtime is null");
    }
    if (model == nullptr) {
        return invalid_argument("model is null");
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
        const auto analysis = analyze_with_runtime(*runtime, core_text, pipeline_options.language);
        if (!model_supports_analysis_languages(model->core, analysis)) {
            set_error("model does not support the requested language");
            return LE_ERROR_UNSUPPORTED_LANGUAGE;
        }
        const auto signals = generate_model_signals(core_text, analysis, model->core);
        auto result =
            make_result(le::core::generate_emphasis(signals, pipeline_options.presentation));
        *out_result = result.release();
        return LE_OK;
    } catch (const le::plugin::Error& error) {
        set_error(error.what());
        return error.status();
    } catch (const le::model::ArtifactError& error) {
        set_error(error.what());
        return LE_ERROR_MODEL_INCOMPATIBLE;
    } catch (const le::core::InvalidUtf8& error) {
        set_error(error.what());
        return LE_ERROR_INVALID_UTF8;
    } catch (const std::bad_alloc&) {
        set_error("could not allocate model processing result");
        return LE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error(error.what());
        return LE_ERROR_INTERNAL;
    } catch (...) {
        set_error("unexpected failure while processing with a model");
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
    case LE_ERROR_PLUGIN_INCOMPATIBLE:
        return "LE_ERROR_PLUGIN_INCOMPATIBLE";
    case LE_ERROR_UNSUPPORTED:
        return "LE_ERROR_UNSUPPORTED";
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
