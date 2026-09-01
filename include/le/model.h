#ifndef LE_MODEL_H
#define LE_MODEL_H

#include "le/analysis.h"
#include "le/types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct le_model le_model_t;

typedef uint32_t le_model_type_t;
#define LE_MODEL_PREFIX ((le_model_type_t)1u)
#define LE_MODEL_LEXICAL_CORE ((le_model_type_t)2u)
#define LE_MODEL_LINEAR_SALIENCE ((le_model_type_t)3u)
#define LE_MODEL_SEGMENTAL_SALIENCE ((le_model_type_t)4u)

#define LE_MODEL_FORMAT_VERSION_MAJOR 1u
#define LE_MODEL_FORMAT_VERSION_MINOR 0u

#ifdef __cplusplus
}
#endif

#endif
