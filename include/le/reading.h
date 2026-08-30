#ifndef LE_READING_H
#define LE_READING_H

#include "le/types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t le_prefix_strategy_t;
#define LE_PREFIX_PROPORTIONAL ((le_prefix_strategy_t)1u)
#define LE_PREFIX_FIXED ((le_prefix_strategy_t)2u)

typedef uint32_t le_reading_model_t;
#define LE_READING_MODEL_PREFIX ((le_reading_model_t)1u)
#define LE_READING_MODEL_LEXICAL_CORE ((le_reading_model_t)2u)

typedef struct le_prefix_model_config {
    uint32_t struct_size;
    uint32_t flags;
    le_prefix_strategy_t strategy;
    uint32_t fixed_graphemes;
    float proportion;
    uint32_t reserved;
} le_prefix_model_config_t;

#define LE_PREFIX_MODEL_CONFIG_V1_SIZE ((uint32_t)sizeof(le_prefix_model_config_t))

typedef struct le_reading_signal {
    le_text_span_t span;
    float fixation_salience;
    float lexical_salience;
    float reading_difficulty;
    uint32_t reserved;
} le_reading_signal_t;

#ifdef __cplusplus
}
#endif

#endif
