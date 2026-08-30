#include "le/provider.h"

#include <stddef.h>

static int supports(void* context, le_string_view_t language) {
    (void)context;
    return language.size >= 2 && language.data[0] == 'x' && language.data[1] == 'd';
}

static le_status_t analyze(void* context, le_string_view_t text, le_string_view_t language,
                           const le_analysis_sink_v1_t* sink) {
    le_status_t status;
    (void)context;
    if (sink == NULL || sink->struct_size < LE_ANALYSIS_SINK_V1_SIZE || sink->flags != 0) {
        return LE_ERROR_PLUGIN_FAILURE;
    }
    status = sink->add_node(sink->context, 0, LE_NODE_DOCUMENT,
                            (le_text_span_t){0, (uint64_t)text.size});
    if (status != LE_OK)
        return status;
    status =
        sink->add_node(sink->context, 1, LE_NODE_UNIT, (le_text_span_t){0, (uint64_t)text.size});
    if (status != LE_OK)
        return status;
    status = sink->add_child(sink->context, 0, 1);
    if (status != LE_OK)
        return status;
    status = sink->add_feature(sink->context, 1, LE_FEATURE_RANGE_VENDOR_BEGIN, 0.75F);
    if (status != LE_OK)
        return status;
    return sink->add_language_region(sink->context, (le_text_span_t){0, (uint64_t)text.size},
                                     language, 1.0F);
}

static const le_provider_v1_t provider = {
    LE_PROVIDER_V1_SIZE,
    LE_PROVIDER_ABI_VERSION,
    LE_PROVIDER_FLAG_THREAD_SAFE,
    0,
    {"test-dynamic", 12},
    NULL,
    supports,
    analyze,
    NULL,
};

LE_PROVIDER_EXPORT const le_provider_v1_t* le_provider_entry_v1(void) { return &provider; }
