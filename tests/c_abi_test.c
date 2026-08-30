#include "le/api.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(le_status_t) == sizeof(int32_t), "le_status_t must be int32_t-sized");
_Static_assert(offsetof(le_text_span_t, begin) == 0, "span begin must be first");
_Static_assert(sizeof(le_feature_t) == 8, "feature ABI layout changed");
_Static_assert(sizeof(le_model_type_t) == sizeof(uint32_t), "model type must be uint32_t-sized");
_Static_assert(LE_MODEL_LINEAR_SALIENCE == 3u, "unexpected learned model type");
_Static_assert(sizeof(le_reading_signal_t) == 32, "reading signal ABI layout changed");
_Static_assert(offsetof(le_analysis_node_t, span) == 8, "analysis node ABI layout changed");
_Static_assert(LE_PROCESS_OPTIONS_V1_SIZE == offsetof(le_process_options_t, presentation_policy),
               "v1 process options size changed");
_Static_assert(LE_PROCESS_OPTIONS_V2_SIZE == offsetof(le_process_options_t, reading_model),
               "v2 process options size changed");
_Static_assert(LE_ABI_VERSION == ((1u << 16u) | 7u), "unexpected ABI version");
_Static_assert(LE_PROVIDER_ABI_VERSION == (1u << 16u), "unexpected provider ABI version");

static const uint8_t lexical_model_fixture[] = {
    0x4c, 0x41, 0x45, 0x4d, 0x4f, 0x44, 0x4c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x4c, 0x00, 0x00, 0x00, 0xb4, 0xd8, 0x85, 0x1e, 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x65, 0x6e, 0x02, 0x00, 0x7a, 0x68, 0x01, 0x00, 0x01, 0x00,
};

typedef struct le_process_options_v1_fixture {
    uint32_t struct_size;
    uint32_t flags;
    le_string_view_t language;
    le_prefix_strategy_t prefix_strategy;
    uint32_t fixed_graphemes;
    float prefix_proportion;
    float emphasis_strength;
} le_process_options_v1_fixture_t;

typedef struct le_process_options_v2_fixture {
    uint32_t struct_size;
    uint32_t flags;
    le_string_view_t language;
    le_prefix_strategy_t prefix_strategy;
    uint32_t fixed_graphemes;
    float prefix_proportion;
    float emphasis_strength;
    le_presentation_policy_t presentation_policy;
    float minimum_emphasis_strength;
    float salience_threshold;
} le_process_options_v2_fixture_t;

_Static_assert(sizeof(le_process_options_v1_fixture_t) == LE_PROCESS_OPTIONS_V1_SIZE,
               "legacy process-options fixture does not match ABI v1");
_Static_assert(sizeof(le_process_options_v2_fixture_t) == LE_PROCESS_OPTIONS_V2_SIZE,
               "legacy process-options fixture does not match ABI v2");

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition))                                                                          \
            return __LINE__;                                                                       \
    } while (0)

int main(void) {
    le_runtime_config_t config;
    le_process_options_t options;
    le_prefix_model_config_t model_config;
    le_presentation_config_t presentation;
    le_runtime_t* runtime = NULL;
    le_model_t* model = NULL;
    le_model_t* invalid_model = (le_model_t*)0x1;
    le_result_t* result = NULL;
    le_analysis_t* analysis = NULL;
    le_analysis_t* english_analysis = NULL;
    le_analysis_t* chinese_analysis = NULL;
    le_signal_result_t* signals = NULL;
    le_signal_result_t* lexical_signals = NULL;
    le_signal_result_t* chinese_signals = NULL;
    le_signal_result_t* model_signals = NULL;
    le_result_t* staged_result = NULL;
    le_result_t* legacy_result = NULL;
    le_result_t* model_result = NULL;
    const char input[] = "hello 世界";
    const le_emphasis_t* data;
    const le_analysis_node_t* nodes;
    const le_language_region_t* regions;
    const le_reading_signal_t* signal_data;

    le_runtime_config_init(&config);
    le_process_options_init(&options);
    le_prefix_model_config_init(&model_config);
    le_presentation_config_init(&presentation);
    options.prefix_strategy = LE_PREFIX_FIXED;
    options.fixed_graphemes = 1;

    CHECK(config.struct_size == sizeof(config));
    CHECK(options.struct_size == sizeof(options));
    CHECK(model_config.struct_size == sizeof(model_config));
    CHECK(presentation.struct_size == sizeof(presentation));
    CHECK(le_runtime_create(&config, &runtime) == LE_OK);
    CHECK(le_runtime_provider_count(NULL) == 0);
    CHECK(le_runtime_provider_count(runtime) == 0);
    CHECK(le_runtime_provider_name_at(runtime, 0).data == NULL);
    CHECK(le_runtime_register_provider(runtime, NULL) == LE_ERROR_INVALID_ARGUMENT);
    CHECK(le_runtime_load_provider(runtime, (le_string_view_t){NULL, 0}) ==
          LE_ERROR_INVALID_ARGUMENT);
    CHECK(le_runtime_dynamic_providers_enabled() == 0 ||
          le_runtime_dynamic_providers_enabled() == 1);
    if (!le_runtime_dynamic_providers_enabled()) {
        CHECK(le_runtime_load_provider(runtime, (le_string_view_t){"provider", 8}) ==
              LE_ERROR_UNSUPPORTED);
    }
    CHECK(strcmp(le_status_string(LE_ERROR_PLUGIN_INCOMPATIBLE), "LE_ERROR_PLUGIN_INCOMPATIBLE") ==
          0);
    CHECK(strcmp(le_status_string(LE_ERROR_UNSUPPORTED), "LE_ERROR_UNSUPPORTED") == 0);
    CHECK(le_model_load(runtime, NULL, 0, &invalid_model) == LE_ERROR_INVALID_ARGUMENT);
    CHECK(invalid_model == NULL);
    CHECK(le_model_load(runtime, lexical_model_fixture, sizeof(lexical_model_fixture), &model) ==
          LE_OK);
    CHECK(le_model_type(model) == LE_MODEL_LEXICAL_CORE);
    CHECK(le_model_version(model) == 3);
    CHECK(le_model_minimum_abi_version(model) == ((1u << 16u) | 4u));
    CHECK(le_model_language_count(model) == 2);
    CHECK(le_model_language_at(model, 0).size == 2);
    CHECK(memcmp(le_model_language_at(model, 0).data, "en", 2) == 0);
    CHECK(le_model_supports_language(model, (le_string_view_t){"en-US", 5}) == 1);
    CHECK(le_model_supports_language(model, (le_string_view_t){"ja", 2}) == 0);
    CHECK(le_model_required_feature_count(model) == 1);
    CHECK(le_model_required_feature_data(model)[0] == LE_FEATURE_LEXICAL_CORE);
    {
        uint8_t corrupted[sizeof(lexical_model_fixture)];
        memcpy(corrupted, lexical_model_fixture, sizeof(corrupted));
        corrupted[sizeof(corrupted) - 1] ^= 1u;
        CHECK(le_model_load(runtime, corrupted, sizeof(corrupted), &invalid_model) ==
              LE_ERROR_MODEL_INVALID);
        CHECK(invalid_model == NULL);
    }
    CHECK(le_process(runtime, (le_string_view_t){input, sizeof(input) - 1}, &options, &result) ==
          LE_OK);
    CHECK(le_result_emphasis_count(result) == 2);
    data = le_result_emphasis_data(result);
    CHECK(data != NULL);
    CHECK(data[0].span.begin == 0 && data[0].span.end == 1);
    CHECK(data[1].span.begin == 6 && data[1].span.end == 9);

    {
        const le_process_options_v1_fixture_t legacy = {
            LE_PROCESS_OPTIONS_V1_SIZE, 0, {NULL, 0}, LE_PREFIX_FIXED, 1, 0.5F, 0.5F,
        };
        CHECK(le_process(runtime, (le_string_view_t){input, sizeof(input) - 1},
                         (const le_process_options_t*)&legacy, &legacy_result) == LE_OK);
        CHECK(le_result_emphasis_count(legacy_result) == 2);
        CHECK(le_result_emphasis_data(legacy_result)[0].strength == 0.5F);
        le_result_destroy(legacy_result);
        legacy_result = NULL;
    }

    {
        const le_process_options_v2_fixture_t legacy = {
            LE_PROCESS_OPTIONS_V2_SIZE,  0,     {NULL, 0}, LE_PREFIX_FIXED, 1, 0.5F, 0.75F,
            LE_POLICY_VARIABLE_STRENGTH, 0.25F, 0.0F,
        };
        CHECK(le_process(runtime, (le_string_view_t){input, sizeof(input) - 1},
                         (const le_process_options_t*)&legacy, &legacy_result) == LE_OK);
        CHECK(le_result_emphasis_count(legacy_result) == 2);
        CHECK(le_result_emphasis_data(legacy_result)[0].strength == 0.75F);
        le_result_destroy(legacy_result);
        legacy_result = NULL;
    }

    CHECK(le_analyze(runtime, (le_string_view_t){input, sizeof(input) - 1},
                     (le_string_view_t){NULL, 0}, &analysis) == LE_OK);
    CHECK(le_analysis_node_count(analysis) == 3);
    CHECK(le_analysis_child_count(analysis) == 2);
    CHECK(le_analysis_feature_count(analysis) == 5);
    nodes = le_analysis_node_data(analysis);
    CHECK(nodes != NULL && nodes[0].kind == LE_NODE_DOCUMENT);
    CHECK(nodes[0].child_count == 2 && nodes[1].kind == LE_NODE_UNIT);
    CHECK(le_analysis_language_region_count(analysis) == 1);
    regions = le_analysis_language_region_data(analysis);
    CHECK(regions != NULL && regions[0].language.size == 3);
    CHECK(memcmp(regions[0].language.data, "und", 3) == 0);
    CHECK(le_generate_model_signals(runtime, analysis, model, &model_signals) ==
          LE_ERROR_UNSUPPORTED_LANGUAGE);
    CHECK(model_signals == NULL);

    model_config.strategy = LE_PREFIX_FIXED;
    model_config.fixed_graphemes = 2;
    CHECK(le_generate_prefix_signals(runtime, analysis, &model_config, &signals) == LE_OK);
    CHECK(le_signal_result_count(signals) == 4);
    signal_data = le_signal_result_data(signals);
    CHECK(signal_data != NULL);
    CHECK(signal_data[0].span.begin == 0 && signal_data[0].span.end == 1);
    CHECK(signal_data[0].fixation_salience == 1.0F);
    CHECK(signal_data[1].fixation_salience == 0.5F);

    presentation.maximum_strength = 0.75F;
    CHECK(le_generate_emphasis(runtime, signals, &presentation, &staged_result) == LE_OK);
    CHECK(le_result_emphasis_count(staged_result) == 2);
    data = le_result_emphasis_data(staged_result);
    CHECK(data[0].span.begin == 0 && data[0].span.end == 2);
    CHECK(data[0].strength == 0.75F);
    le_result_destroy(staged_result);
    staged_result = NULL;

    presentation.policy = LE_POLICY_VARIABLE_STRENGTH;
    presentation.minimum_strength = 0.2F;
    presentation.maximum_strength = 1.0F;
    CHECK(le_generate_emphasis(runtime, signals, &presentation, &staged_result) == LE_OK);
    CHECK(le_result_emphasis_count(staged_result) == 4);
    data = le_result_emphasis_data(staged_result);
    CHECK(data[0].strength == 1.0F);
    CHECK(data[1].strength > 0.59F && data[1].strength < 0.61F);

    {
        const char english[] = "unbelievable reading";
        CHECK(le_analyze(runtime, (le_string_view_t){english, sizeof(english) - 1},
                         (le_string_view_t){"en", 2}, &english_analysis) == LE_OK);
        CHECK(le_analysis_node_count(english_analysis) == 9);
        CHECK(le_generate_lexical_core_signals(runtime, english_analysis, &lexical_signals) ==
              LE_OK);
        CHECK(le_signal_result_count(lexical_signals) == 2);
        signal_data = le_signal_result_data(lexical_signals);
        CHECK(signal_data[0].span.begin == 2 && signal_data[0].span.end == 8);
        CHECK(signal_data[0].lexical_salience == 1.0F);
        CHECK(signal_data[1].span.begin == 13 && signal_data[1].span.end == 17);
        CHECK(le_generate_model_signals(runtime, english_analysis, model, &model_signals) == LE_OK);
        CHECK(le_signal_result_count(model_signals) == 2);
        CHECK(le_signal_result_data(model_signals)[0].span.begin == 2);
        {
            le_process_options_t model_options;
            le_process_options_init(&model_options);
            model_options.language = (le_string_view_t){"en", 2};
            CHECK(le_process_with_model(runtime, model,
                                        (le_string_view_t){english, sizeof(english) - 1},
                                        &model_options, &model_result) == LE_OK);
            CHECK(le_result_emphasis_count(model_result) == 2);
        }
    }

    {
        const char chinese[] = "中华人民共和国";
        uint32_t item;
        int found_lexical_core = 0;
        int found_han = 0;
        CHECK(le_analyze(runtime, (le_string_view_t){chinese, sizeof(chinese) - 1},
                         (le_string_view_t){"zh-Hans", 7}, &chinese_analysis) == LE_OK);
        CHECK(le_analysis_node_count(chinese_analysis) == 12);
        nodes = le_analysis_node_data(chinese_analysis);
        CHECK(nodes[0].child_count == 1 && nodes[1].kind == LE_NODE_SENTENCE);
        CHECK(nodes[1].child_count == 3 && nodes[2].kind == LE_NODE_UNIT);
        CHECK(nodes[2].child_count == 2 && nodes[3].kind == LE_NODE_SUBUNIT);
        for (item = 0; item < nodes[2].feature_count; ++item) {
            const le_feature_t feature =
                le_analysis_feature_data(chinese_analysis)[nodes[2].first_feature + item];
            found_lexical_core |= feature.id == LE_FEATURE_LEXICAL_CORE;
            found_han |= feature.id == LE_FEATURE_SCRIPT_HAN;
        }
        CHECK(found_lexical_core && found_han);
        CHECK(le_generate_lexical_core_signals(runtime, chinese_analysis, &chinese_signals) ==
              LE_OK);
        CHECK(le_signal_result_count(chinese_signals) == 3);
        signal_data = le_signal_result_data(chinese_signals);
        CHECK(signal_data[0].span.begin == 0 && signal_data[0].span.end == 6);
        CHECK(signal_data[1].span.begin == 6 && signal_data[1].span.end == 12);
        CHECK(signal_data[2].span.begin == 12 && signal_data[2].span.end == 21);
    }

    le_runtime_destroy(runtime);
    CHECK(le_result_emphasis_count(result) == 2);
    CHECK(le_analysis_node_count(analysis) == 3);
    CHECK(le_signal_result_count(signals) == 4);
    CHECK(le_signal_result_count(lexical_signals) == 2);
    CHECK(le_signal_result_count(chinese_signals) == 3);
    CHECK(le_model_version(model) == 3);
    le_result_destroy(staged_result);
    le_signal_result_destroy(lexical_signals);
    le_signal_result_destroy(chinese_signals);
    le_signal_result_destroy(model_signals);
    le_signal_result_destroy(signals);
    le_analysis_destroy(english_analysis);
    le_analysis_destroy(chinese_analysis);
    le_analysis_destroy(analysis);
    le_result_destroy(result);
    le_result_destroy(model_result);
    le_model_destroy(model);
    CHECK(le_model_type(NULL) == 0);
    CHECK(le_model_language_count(NULL) == 0);
    CHECK(le_model_language_at(NULL, 0).data == NULL);
    le_model_destroy(NULL);
    return 0;
}
