#ifndef LE_ANALYSIS_H
#define LE_ANALYSIS_H

#include "le/types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t le_node_id_t;
typedef uint32_t le_node_kind_t;
typedef uint32_t le_feature_id_t;

#define LE_NODE_DOCUMENT ((le_node_kind_t)1u)
#define LE_NODE_BLOCK ((le_node_kind_t)2u)
#define LE_NODE_PARAGRAPH ((le_node_kind_t)3u)
#define LE_NODE_SENTENCE ((le_node_kind_t)4u)
#define LE_NODE_UNIT ((le_node_kind_t)5u)
#define LE_NODE_SUBUNIT ((le_node_kind_t)6u)

/* Stable feature namespaces. Values inside a namespace remain extensible. */
#define LE_FEATURE_RANGE_CORE_BEGIN ((le_feature_id_t)0x00000000u)
#define LE_FEATURE_RANGE_MORPHOLOGY_BEGIN ((le_feature_id_t)0x00010000u)
#define LE_FEATURE_RANGE_SYNTAX_BEGIN ((le_feature_id_t)0x00020000u)
#define LE_FEATURE_RANGE_SEMANTIC_BEGIN ((le_feature_id_t)0x00030000u)
#define LE_FEATURE_RANGE_SCRIPT_BEGIN ((le_feature_id_t)0x00040000u)
#define LE_FEATURE_RANGE_VENDOR_BEGIN ((le_feature_id_t)0x80000000u)

#define LE_FEATURE_BOUNDARY_STRENGTH ((le_feature_id_t)0x00000001u)
#define LE_FEATURE_GRAPHEME_COUNT ((le_feature_id_t)0x00000002u)
#define LE_FEATURE_LEXICAL_CORE ((le_feature_id_t)0x00010001u)
#define LE_FEATURE_DERIVATIONAL_AFFIX ((le_feature_id_t)0x00010002u)
#define LE_FEATURE_GRAMMATICAL_AFFIX ((le_feature_id_t)0x00010003u)
#define LE_FEATURE_CONTENT_UNIT ((le_feature_id_t)0x00030001u)

/*
 * A node in an immutable analysis. Child and feature ranges index the arrays
 * returned by le_analysis_child_data and le_analysis_feature_data.
 */
typedef struct le_analysis_node {
    le_node_id_t id;
    le_node_kind_t kind;
    le_text_span_t span;
    uint32_t first_child;
    uint32_t child_count;
    uint32_t first_feature;
    uint32_t feature_count;
} le_analysis_node_t;

typedef struct le_feature {
    le_feature_id_t id;
    float value;
} le_feature_t;

typedef struct le_language_region {
    le_text_span_t span;
    le_string_view_t language;
    float confidence;
    uint32_t reserved;
} le_language_region_t;

#ifdef __cplusplus
}
#endif

#endif
