#include "le/api.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(le_status_t) == sizeof(int32_t), "le_status_t must be int32_t-sized");
_Static_assert(offsetof(le_text_span_t, begin) == 0, "span begin must be first");

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
    const char input[] = "hello 世界";
    const le_emphasis_t* data;

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

    le_runtime_destroy(runtime);
    CHECK(le_result_emphasis_count(result) == 2);
    le_result_destroy(result);
    return 0;
}
