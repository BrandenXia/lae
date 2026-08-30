#include "le/api.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(le_status_t) == sizeof(int32_t), "le_status_t must be int32_t-sized");
_Static_assert(offsetof(le_text_span_t, begin) == 0, "span begin must be first");
_Static_assert(sizeof(le_feature_t) == 8, "feature ABI layout changed");
_Static_assert(offsetof(le_analysis_node_t, span) == 8, "analysis node ABI layout changed");
_Static_assert(LE_ABI_VERSION == ((1u << 16u) | 1u), "unexpected ABI version");

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition))                                                                          \
            return __LINE__;                                                                       \
    } while (0)

int main(void) {
    le_runtime_config_t config;
    le_process_options_t options;
    le_runtime_t* runtime = NULL;
    le_result_t* result = NULL;
    le_analysis_t* analysis = NULL;
    const char input[] = "hello 世界";
    const le_emphasis_t* data;
    const le_analysis_node_t* nodes;
    const le_language_region_t* regions;

    le_runtime_config_init(&config);
    le_process_options_init(&options);
    options.prefix_strategy = LE_PREFIX_FIXED;
    options.fixed_graphemes = 1;

    CHECK(config.struct_size == sizeof(config));
    CHECK(options.struct_size == sizeof(options));
    CHECK(le_runtime_create(&config, &runtime) == LE_OK);
    CHECK(le_process(runtime, (le_string_view_t){input, sizeof(input) - 1}, &options, &result) ==
          LE_OK);
    CHECK(le_result_emphasis_count(result) == 2);
    data = le_result_emphasis_data(result);
    CHECK(data != NULL);
    CHECK(data[0].span.begin == 0 && data[0].span.end == 1);
    CHECK(data[1].span.begin == 6 && data[1].span.end == 9);

    CHECK(le_analyze(runtime, (le_string_view_t){input, sizeof(input) - 1},
                     (le_string_view_t){"en-US", 5}, &analysis) == LE_OK);
    CHECK(le_analysis_node_count(analysis) == 3);
    CHECK(le_analysis_child_count(analysis) == 2);
    CHECK(le_analysis_feature_count(analysis) == 5);
    nodes = le_analysis_node_data(analysis);
    CHECK(nodes != NULL && nodes[0].kind == LE_NODE_DOCUMENT);
    CHECK(nodes[0].child_count == 2 && nodes[1].kind == LE_NODE_UNIT);
    CHECK(le_analysis_language_region_count(analysis) == 1);
    regions = le_analysis_language_region_data(analysis);
    CHECK(regions != NULL && regions[0].language.size == 5);
    CHECK(memcmp(regions[0].language.data, "en-US", 5) == 0);

    le_runtime_destroy(runtime);
    CHECK(le_result_emphasis_count(result) == 2);
    CHECK(le_analysis_node_count(analysis) == 3);
    le_analysis_destroy(analysis);
    le_result_destroy(result);
    return 0;
}
