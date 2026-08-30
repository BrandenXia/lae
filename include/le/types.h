#ifndef LE_TYPES_H
#define LE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t le_status_t;

#define LE_OK ((le_status_t)0)
#define LE_ERROR_INVALID_ARGUMENT ((le_status_t) - 1)
#define LE_ERROR_INVALID_UTF8 ((le_status_t) - 2)
#define LE_ERROR_OUT_OF_MEMORY ((le_status_t) - 3)
#define LE_ERROR_UNSUPPORTED_LANGUAGE ((le_status_t) - 4)
#define LE_ERROR_MODEL_INVALID ((le_status_t) - 5)
#define LE_ERROR_MODEL_INCOMPATIBLE ((le_status_t) - 6)
#define LE_ERROR_PLUGIN_FAILURE ((le_status_t) - 7)
#define LE_ERROR_INTERNAL ((le_status_t) - 8)

/* A borrowed byte view. The data need not be null terminated. */
typedef struct le_string_view {
    const char* data;
    size_t size;
} le_string_view_t;

/* A half-open [begin, end) range of UTF-8 byte offsets. */
typedef struct le_text_span {
    uint64_t begin;
    uint64_t end;
} le_text_span_t;

/* A normalized, ordered, non-overlapping presentation instruction. */
typedef struct le_emphasis {
    le_text_span_t span;
    float strength;
    uint32_t style_class;
} le_emphasis_t;

#ifdef __cplusplus
}
#endif

#endif
