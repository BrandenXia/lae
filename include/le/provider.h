#ifndef LE_PROVIDER_H
#define LE_PROVIDER_H

#include "le/analysis.h"
#include "le/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LE_PROVIDER_ABI_VERSION_MAJOR 1u
#define LE_PROVIDER_ABI_VERSION_MINOR 0u
#define LE_PROVIDER_ABI_VERSION                                                                    \
    ((LE_PROVIDER_ABI_VERSION_MAJOR << 16u) | LE_PROVIDER_ABI_VERSION_MINOR)

#define LE_PROVIDER_FLAG_THREAD_SAFE (1u << 0u)

typedef struct le_analysis_sink_v1 le_analysis_sink_v1_t;

typedef le_status_t (*le_provider_add_node_v1_fn)(void* context, le_node_id_t id,
                                                  le_node_kind_t kind, le_text_span_t span);
typedef le_status_t (*le_provider_add_child_v1_fn)(void* context, le_node_id_t parent,
                                                   le_node_id_t child);
typedef le_status_t (*le_provider_add_feature_v1_fn)(void* context, le_node_id_t node,
                                                     le_feature_id_t feature, float value);
typedef le_status_t (*le_provider_add_language_region_v1_fn)(void* context, le_text_span_t span,
                                                             le_string_view_t language,
                                                             float confidence);

/*
 * Runtime-owned analysis builder passed to provider callbacks. Providers must
 * use these functions instead of retaining the sink or allocating IR storage.
 */
struct le_analysis_sink_v1 {
    uint32_t struct_size;
    uint32_t flags;
    void* context;
    le_provider_add_node_v1_fn add_node;
    le_provider_add_child_v1_fn add_child;
    le_provider_add_feature_v1_fn add_feature;
    le_provider_add_language_region_v1_fn add_language_region;
};

#define LE_ANALYSIS_SINK_V1_SIZE ((uint32_t)sizeof(le_analysis_sink_v1_t))

typedef int (*le_provider_supports_v1_fn)(void* context, le_string_view_t language);
typedef le_status_t (*le_provider_analyze_v1_fn)(void* context, le_string_view_t text,
                                                 le_string_view_t language,
                                                 const le_analysis_sink_v1_t* sink);
typedef void (*le_provider_destroy_v1_fn)(void* context);

/* A v1 provider descriptor. The runtime copies it during registration. */
typedef struct le_provider_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t flags;
    uint32_t reserved;
    le_string_view_t name;
    void* context;
    le_provider_supports_v1_fn supports;
    le_provider_analyze_v1_fn analyze;
    le_provider_destroy_v1_fn destroy;
} le_provider_v1_t;

#define LE_PROVIDER_V1_SIZE ((uint32_t)sizeof(le_provider_v1_t))
#define LE_PROVIDER_ENTRY_V1_NAME "le_provider_entry_v1"

typedef const le_provider_v1_t* (*le_provider_entry_v1_fn)(void);

#if defined(_WIN32)
#define LE_PROVIDER_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define LE_PROVIDER_EXPORT __attribute__((visibility("default")))
#else
#define LE_PROVIDER_EXPORT
#endif

#ifdef __cplusplus
}
#endif

#endif
