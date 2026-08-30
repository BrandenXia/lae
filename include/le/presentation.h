#ifndef LE_PRESENTATION_H
#define LE_PRESENTATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t le_presentation_policy_t;
#define LE_POLICY_BINARY ((le_presentation_policy_t)1u)
#define LE_POLICY_VARIABLE_STRENGTH ((le_presentation_policy_t)2u)

typedef struct le_presentation_config {
    uint32_t struct_size;
    uint32_t flags;
    le_presentation_policy_t policy;
    float salience_threshold;
    float minimum_strength;
    float maximum_strength;
} le_presentation_config_t;

#define LE_PRESENTATION_CONFIG_V1_SIZE ((uint32_t)sizeof(le_presentation_config_t))

#ifdef __cplusplus
}
#endif

#endif
