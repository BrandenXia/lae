#include "le/api.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Expected {
    std::uint64_t begin;
    std::uint64_t end;
};

int failures = 0;
int destroyed_provider_contexts = 0;

struct StaticProviderContext {
    std::atomic<int> active{0};
    std::atomic<int> maximum_active{0};
};

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

int static_provider_supports(void*, le_string_view_t language) {
    return language.size >= 2 && language.data[0] == 'x' && language.data[1] == 's';
}

int invalid_provider_supports(void*, le_string_view_t language) {
    return language.size == 2 && language.data[0] == 'x' && language.data[1] == 'i';
}

le_status_t static_provider_analyze(void* context, le_string_view_t text, le_string_view_t language,
                                    const le_analysis_sink_v1_t* sink) {
    auto* state = static_cast<StaticProviderContext*>(context);
    if (state != nullptr) {
        const auto active = state->active.fetch_add(1) + 1;
        auto maximum = state->maximum_active.load();
        while (maximum < active && !state->maximum_active.compare_exchange_weak(maximum, active)) {
        }
        std::this_thread::yield();
    }
    const auto finish = [&] {
        if (state != nullptr) {
            state->active.fetch_sub(1);
        }
    };
    if (sink == nullptr || sink->struct_size < LE_ANALYSIS_SINK_V1_SIZE || sink->flags != 0) {
        finish();
        return LE_ERROR_PLUGIN_FAILURE;
    }
    auto status = sink->add_node(sink->context, 0, LE_NODE_DOCUMENT,
                                 le_text_span_t{0, static_cast<std::uint64_t>(text.size)});
    if (status != LE_OK) {
        finish();
        return status;
    }
    status = sink->add_node(sink->context, 1, LE_NODE_UNIT,
                            le_text_span_t{0, static_cast<std::uint64_t>(text.size)});
    if (status != LE_OK) {
        finish();
        return status;
    }
    status = sink->add_child(sink->context, 0, 1);
    if (status != LE_OK) {
        finish();
        return status;
    }
    status = sink->add_feature(sink->context, 1, LE_FEATURE_RANGE_VENDOR_BEGIN, 0.5F);
    if (status != LE_OK) {
        finish();
        return status;
    }
    status = sink->add_language_region(
        sink->context, le_text_span_t{0, static_cast<std::uint64_t>(text.size)}, language, 1.0F);
    finish();
    return status;
}

le_status_t invalid_provider_analyze(void*, le_string_view_t text, le_string_view_t,
                                     const le_analysis_sink_v1_t* sink) {
    return sink->add_node(sink->context, 0, LE_NODE_UNIT,
                          le_text_span_t{0, static_cast<std::uint64_t>(text.size)});
}

void destroy_static_provider(void* context) {
    delete static_cast<StaticProviderContext*>(context);
    ++destroyed_provider_contexts;
}

std::vector<le_emphasis_t> run(std::string_view text, le_process_options_t* options = nullptr,
                               le_status_t* status_out = nullptr) {
    le_runtime_t* runtime = nullptr;
    le_result_t* result = nullptr;
    const auto create_status = le_runtime_create(nullptr, &runtime);
    check(create_status == LE_OK, "runtime creation succeeds");
    const auto status =
        le_process(runtime, le_string_view_t{text.data(), text.size()}, options, &result);
    if (status_out != nullptr) {
        *status_out = status;
    }
    std::vector<le_emphasis_t> output;
    if (status == LE_OK) {
        const auto count = le_result_emphasis_count(result);
        const auto* data = le_result_emphasis_data(result);
        if (count != 0) {
            output.assign(data, data + count);
        }
    }
    le_result_destroy(result);
    le_runtime_destroy(runtime);
    return output;
}

void expect_spans(std::string_view name, std::string_view text, std::vector<Expected> expected,
                  le_process_options_t* options = nullptr) {
    const auto actual = run(text, options);
    check(actual.size() == expected.size(), std::string(name) + ": emphasis count");
    const auto size = std::min(actual.size(), expected.size());
    for (std::size_t index = 0; index < size; ++index) {
        check(actual[index].span.begin == expected[index].begin,
              std::string(name) + ": begin offset");
        check(actual[index].span.end == expected[index].end, std::string(name) + ": end offset");
        check(actual[index].span.begin <= actual[index].span.end,
              std::string(name) + ": ordered range");
        check(actual[index].span.end <= text.size(), std::string(name) + ": range within text");
        check(actual[index].strength == 1.0F, std::string(name) + ": default strength");
        if (index != 0) {
            check(actual[index - 1].span.end <= actual[index].span.begin,
                  std::string(name) + ": non-overlapping ordered spans");
        }
    }
}

void unicode_golden_tests() {
    expect_spans("ASCII", "reading", {{0, 4}});
    expect_spans("precomposed", "éclair", {{0, 4}});
    expect_spans("combining accent", "e\u0301clair", {{0, 5}});
    expect_spans("emoji ZWJ", "👩‍🚀abc", {{0, 12}});
    expect_spans("Chinese", "中华人民共和国", {{0, 12}});
    expect_spans("Japanese", "食べました", {{0, 9}});
    expect_spans("mixed scripts", "我a 👩‍🚀", {{0, 3}, {5, 16}});
    expect_spans("punctuation units", "alpha,beta", {{0, 3}, {6, 8}});

    le_process_options_t fixed;
    le_process_options_init(&fixed);
    fixed.prefix_strategy = LE_PREFIX_FIXED;
    fixed.fixed_graphemes = 1;
    expect_spans("Arabic combining", "شّمس", {{0, 4}}, &fixed);
    expect_spans("Devanagari conjunct", "क्षम", {{0, 9}}, &fixed);
    expect_spans("emoji modifier", "👍🏽ok", {{0, 8}}, &fixed);
}

void contract_tests() {
    expect_spans("empty", "", {});
    expect_spans("only separators", " \n—", {});

    le_process_options_t fixed_zero;
    le_process_options_init(&fixed_zero);
    fixed_zero.prefix_strategy = LE_PREFIX_FIXED;
    fixed_zero.fixed_graphemes = 0;
    expect_spans("fixed zero", "text", {}, &fixed_zero);

    le_process_options_t custom;
    le_process_options_init(&custom);
    custom.emphasis_strength = 0.25F;
    auto custom_result = run("reading", &custom);
    check(custom_result.size() == 1 && std::fabs(custom_result[0].strength - 0.25F) < 0.0001F,
          "custom emphasis strength");

    le_process_options_t legacy;
    le_process_options_init(&legacy);
    legacy.struct_size = LE_PROCESS_OPTIONS_V1_SIZE;
    legacy.presentation_policy = 999;
    legacy.minimum_emphasis_strength = 0.9F;
    const auto legacy_result = run("reading", &legacy);
    check(legacy_result.size() == 1 && legacy_result[0].span.end == 4,
          "v1 process options ignore appended policy fields");

    le_process_options_t variable;
    le_process_options_init(&variable);
    variable.presentation_policy = LE_POLICY_VARIABLE_STRENGTH;
    variable.minimum_emphasis_strength = 0.2F;
    const auto variable_result = run("reading", &variable);
    check(variable_result.size() == 4, "variable policy emits distinct-strength spans");
    check(std::fabs(variable_result.front().strength - 1.0F) < 0.0001F,
          "variable policy starts at maximum strength");
    check(std::fabs(variable_result.back().strength - 0.6F) < 0.0001F,
          "variable policy interpolates final selected grapheme");

    le_process_options_t threshold;
    le_process_options_init(&threshold);
    threshold.salience_threshold = 0.75F;
    const auto threshold_result = run("reading", &threshold);
    check(threshold_result.size() == 1 && threshold_result[0].span.end == 2,
          "binary policy applies salience threshold before merging");

    le_process_options_t lexical_core;
    le_process_options_init(&lexical_core);
    lexical_core.language = le_string_view_t{"en", 2};
    lexical_core.reading_model = LE_READING_MODEL_LEXICAL_CORE;
    expect_spans("English lexical cores", "unbelievable reading", {{2, 8}, {13, 17}},
                 &lexical_core);

    le_process_options_t chinese_lexical_core;
    le_process_options_init(&chinese_lexical_core);
    chinese_lexical_core.language = le_string_view_t{"zh-Hans", 7};
    chinese_lexical_core.reading_model = LE_READING_MODEL_LEXICAL_CORE;
    expect_spans("Chinese lexical cores", "中华人民共和国", {{0, 21}}, &chinese_lexical_core);

    le_process_options_t chinese_prefix;
    le_process_options_init(&chinese_prefix);
    chinese_prefix.language = le_string_view_t{"zh-Hans", 7};
    expect_spans("Chinese segmented prefix", "中华人民共和国", {{0, 3}, {6, 9}, {12, 18}},
                 &chinese_prefix);

    le_process_options_t legacy_v2;
    le_process_options_init(&legacy_v2);
    legacy_v2.struct_size = LE_PROCESS_OPTIONS_V2_SIZE;
    legacy_v2.language = le_string_view_t{"en", 2};
    legacy_v2.reading_model = LE_READING_MODEL_LEXICAL_CORE;
    expect_spans("v2 ignores reading model", "reading", {{0, 4}}, &legacy_v2);

    le_process_options_t invalid_model;
    le_process_options_init(&invalid_model);
    invalid_model.reading_model = 999;
    le_status_t invalid_model_status = LE_OK;
    run("reading", &invalid_model, &invalid_model_status);
    check(invalid_model_status == LE_ERROR_INVALID_ARGUMENT, "invalid reading model rejected");

    const std::string invalid("\xF0\x28\x8C\x28", 4);
    le_status_t invalid_status = LE_OK;
    const auto invalid_output = run(invalid, nullptr, &invalid_status);
    check(invalid_status == LE_ERROR_INVALID_UTF8, "invalid UTF-8 status");
    check(invalid_output.empty(), "invalid UTF-8 has no output");

    le_runtime_t* runtime = nullptr;
    check(le_runtime_create(nullptr, &runtime) == LE_OK, "runtime for argument tests");
    le_result_t* result = reinterpret_cast<le_result_t*>(0x1);
    check(le_process(runtime, le_string_view_t{nullptr, 1}, nullptr, &result) ==
              LE_ERROR_INVALID_ARGUMENT,
          "null text with nonzero size rejected");
    check(result == nullptr, "out_result cleared on error");

    le_process_options_t invalid_options;
    le_process_options_init(&invalid_options);
    invalid_options.prefix_proportion = 1.5F;
    check(le_process(runtime, le_string_view_t{"x", 1}, &invalid_options, &result) ==
              LE_ERROR_INVALID_ARGUMENT,
          "invalid proportion rejected");
    const auto detail = le_runtime_last_error(runtime);
    check(detail.data != nullptr && detail.size != 0, "detailed diagnostic available");

    le_analysis_t* analysis = reinterpret_cast<le_analysis_t*>(0x1);
    check(le_analyze(runtime, le_string_view_t{"x", 1}, le_string_view_t{"en_US", 5}, &analysis) ==
              LE_ERROR_INVALID_ARGUMENT,
          "invalid language tag rejected");
    check(analysis == nullptr, "out_analysis cleared on error");
    check(le_analyze(runtime, le_string_view_t{invalid.data(), invalid.size()},
                     le_string_view_t{nullptr, 0}, &analysis) == LE_ERROR_INVALID_UTF8,
          "analysis rejects invalid UTF-8");

    check(le_analysis_node_count(nullptr) == 0, "null analysis node count is zero");
    check(le_analysis_node_data(nullptr) == nullptr, "null analysis node data is null");
    check(le_analysis_child_count(nullptr) == 0, "null analysis child count is zero");
    check(le_analysis_child_data(nullptr) == nullptr, "null analysis child data is null");
    check(le_analysis_feature_count(nullptr) == 0, "null analysis feature count is zero");
    check(le_analysis_feature_data(nullptr) == nullptr, "null analysis feature data is null");
    check(le_analysis_language_region_count(nullptr) == 0, "null analysis region count is zero");
    check(le_analysis_language_region_data(nullptr) == nullptr,
          "null analysis region data is null");
    le_analysis_destroy(nullptr);

    le_signal_result_t* signals = reinterpret_cast<le_signal_result_t*>(0x1);
    check(le_generate_prefix_signals(runtime, nullptr, nullptr, &signals) ==
              LE_ERROR_INVALID_ARGUMENT,
          "signal generation requires analysis");
    check(signals == nullptr, "out_signals cleared on error");
    signals = reinterpret_cast<le_signal_result_t*>(0x1);
    check(le_generate_lexical_core_signals(runtime, nullptr, &signals) == LE_ERROR_INVALID_ARGUMENT,
          "lexical-core generation requires analysis");
    check(signals == nullptr, "lexical-core out_signals cleared on error");
    check(le_signal_result_count(nullptr) == 0, "null signal count is zero");
    check(le_signal_result_data(nullptr) == nullptr, "null signal data is null");
    le_signal_result_destroy(nullptr);

    check(le_analyze(runtime, le_string_view_t{"abc", 3}, le_string_view_t{nullptr, 0},
                     &analysis) == LE_OK,
          "analysis owns source text");
    const auto analysis_text = le_analysis_text(analysis);
    check(analysis_text.size == 3 &&
              std::string_view(analysis_text.data, analysis_text.size) == "abc",
          "analysis exposes immutable source snapshot");
    le_analysis_destroy(analysis);

    check(le_result_emphasis_count(nullptr) == 0, "null result count is zero");
    check(le_result_emphasis_data(nullptr) == nullptr, "null result data is null");
    le_result_destroy(nullptr);
    le_runtime_destroy(runtime);
}

void lifetime_test() {
    le_runtime_t* runtime = nullptr;
    le_result_t* result = nullptr;
    check(le_runtime_create(nullptr, &runtime) == LE_OK, "lifetime runtime creation");
    check(le_process(runtime, le_string_view_t{"hello", 5}, nullptr, &result) == LE_OK,
          "lifetime processing");
    le_runtime_destroy(runtime);
    check(le_result_emphasis_count(result) == 1, "result outlives runtime");
    check(le_result_emphasis_data(result)[0].span.end == 3, "outliving result remains readable");
    le_result_destroy(result);
}

void static_provider_tests() {
    le_runtime_t* runtime = nullptr;
    check(le_runtime_create(nullptr, &runtime) == LE_OK, "plugin runtime creation");

    le_provider_v1_t incompatible{};
    incompatible.struct_size = LE_PROVIDER_V1_SIZE;
    incompatible.abi_version = LE_PROVIDER_ABI_VERSION + 1;
    incompatible.name = le_string_view_t{"future", 6};
    incompatible.supports = static_provider_supports;
    incompatible.analyze = static_provider_analyze;
    check(le_runtime_register_provider(runtime, &incompatible) == LE_ERROR_PLUGIN_INCOMPATIBLE,
          "future provider ABI is rejected");
    check(le_runtime_provider_count(runtime) == 0, "rejected provider is not registered");

    le_provider_v1_t provider{};
    provider.struct_size = LE_PROVIDER_V1_SIZE;
    provider.abi_version = LE_PROVIDER_ABI_VERSION;
    provider.name = le_string_view_t{"test-static", 11};
    auto* provider_context = new StaticProviderContext;
    provider.context = provider_context;
    provider.supports = static_provider_supports;
    provider.analyze = static_provider_analyze;
    provider.destroy = destroy_static_provider;
    check(le_runtime_register_provider(runtime, &provider) == LE_OK,
          "statically linked provider registers");
    check(le_runtime_provider_count(runtime) == 1, "registered provider is discoverable");
    const auto name = le_runtime_provider_name_at(runtime, 0);
    check(std::string_view(name.data, name.size) == "test-static", "provider name is stable");

    le_analysis_t* analysis = nullptr;
    check(le_analyze(runtime, le_string_view_t{"static", 6}, le_string_view_t{"xs", 2},
                     &analysis) == LE_OK,
          "static provider handles supported language");
    check(le_analysis_node_count(analysis) == 2, "static provider graph reaches public IR");
    check(le_analysis_feature_count(analysis) == 1, "static provider feature reaches public IR");
    check(le_analysis_feature_data(analysis)[0].id == LE_FEATURE_RANGE_VENDOR_BEGIN,
          "static provider vendor feature is preserved");
    le_analysis_destroy(analysis);

    le_provider_v1_t invalid_provider{};
    invalid_provider.struct_size = LE_PROVIDER_V1_SIZE;
    invalid_provider.abi_version = LE_PROVIDER_ABI_VERSION;
    invalid_provider.name = le_string_view_t{"test-invalid", 12};
    invalid_provider.supports = invalid_provider_supports;
    invalid_provider.analyze = invalid_provider_analyze;
    check(le_runtime_register_provider(runtime, &invalid_provider) == LE_OK,
          "provider output validation is deferred until analysis");
    analysis = reinterpret_cast<le_analysis_t*>(0x1);
    check(le_analyze(runtime, le_string_view_t{"invalid", 7}, le_string_view_t{"xi", 2},
                     &analysis) == LE_ERROR_PLUGIN_FAILURE,
          "invalid provider graph is rejected at the ABI boundary");
    check(analysis == nullptr, "invalid provider graph produces no analysis handle");

    le_process_options_t options;
    le_process_options_init(&options);
    options.language = le_string_view_t{"xs", 2};
    le_result_t* result = nullptr;
    check(le_process(runtime, le_string_view_t{"static", 6}, &options, &result) == LE_OK,
          "static provider participates in high-level processing");
    check(le_result_emphasis_count(result) == 1, "static provider feeds reading model");
    le_result_destroy(result);

    std::atomic<int> concurrent_failures{0};
    std::vector<std::thread> workers;
    for (int index = 0; index < 8; ++index) {
        workers.emplace_back([&] {
            le_analysis_t* concurrent_analysis = nullptr;
            if (le_analyze(runtime, le_string_view_t{"parallel", 8}, le_string_view_t{"xs", 2},
                           &concurrent_analysis) != LE_OK) {
                ++concurrent_failures;
            }
            le_analysis_destroy(concurrent_analysis);
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    check(concurrent_failures == 0, "concurrent provider analysis succeeds");
    check(provider_context->maximum_active == 1,
          "runtime serializes providers without the thread-safe flag");

    le_runtime_destroy(runtime);
    check(destroyed_provider_contexts == 1, "runtime releases registered provider context");
}

} // namespace

int main() {
    unicode_golden_tests();
    contract_tests();
    lifetime_test();
    static_provider_tests();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All runtime tests passed\n";
    return EXIT_SUCCESS;
}
