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
#include <string_view>
#include <vector>

struct le_runtime {
    std::uint32_t abi_version = LE_ABI_VERSION;
};

struct le_result {
    std::vector<le_emphasis_t> emphasis;
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
    if (!valid_view(source->language)) {
        return invalid_argument("language data is null while language size is nonzero");
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

    target.language = source->language.size == 0
                          ? std::string_view("und")
                          : std::string_view(source->language.data, source->language.size);
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
