#ifndef LE_API_H
#define LE_API_H

#include "le/analysis.h"
#include "le/presentation.h"
#include "le/reading.h"
#include "le/types.h"
#include "le/version.h"

#if defined(_WIN32) && defined(LE_SHARED)
#if defined(LE_BUILDING_RUNTIME)
#define LE_API __declspec(dllexport)
#else
#define LE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define LE_API __attribute__((visibility("default")))
#else
#define LE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct le_runtime le_runtime_t;
typedef struct le_result le_result_t;
typedef struct le_analysis le_analysis_t;
typedef struct le_signal_result le_signal_result_t;

typedef struct le_runtime_config {
    uint32_t struct_size;
    uint32_t flags;
} le_runtime_config_t;

#define LE_RUNTIME_CONFIG_V1_SIZE ((uint32_t)sizeof(le_runtime_config_t))

typedef struct le_process_options {
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
} le_process_options_t;

#define LE_PROCESS_OPTIONS_V1_SIZE ((uint32_t)offsetof(le_process_options_t, presentation_policy))
#define LE_PROCESS_OPTIONS_V2_SIZE ((uint32_t)sizeof(le_process_options_t))

/* Initialize a v1 configuration with default values. */
LE_API void le_runtime_config_init(le_runtime_config_t* config);

/* Initialize the latest options: generic analysis, 50% prefix, binary strength 1.0. */
LE_API void le_process_options_init(le_process_options_t* options);

/* Initialize a v1 prefix model configuration with a 50% proportional prefix. */
LE_API void le_prefix_model_config_init(le_prefix_model_config_t* config);

/* Initialize a v1 binary presentation policy with normalized strength 1.0. */
LE_API void le_presentation_config_init(le_presentation_config_t* config);

/* Create a runtime. config may be NULL. out_runtime must be non-NULL. */
LE_API le_status_t le_runtime_create(const le_runtime_config_t* config, le_runtime_t** out_runtime);

/* Destroy a runtime. NULL is accepted. Existing analyses, signals, and results remain valid. */
LE_API void le_runtime_destroy(le_runtime_t* runtime);

/*
 * Analyze UTF-8 text with the generic provider. Text and language are borrowed
 * only for this call. Empty language means "und". The caller owns out_analysis.
 */
LE_API le_status_t le_analyze(le_runtime_t* runtime, le_string_view_t text,
                              le_string_view_t language, le_analysis_t** out_analysis);

/* Analysis accessors return immutable arrays borrowed until analysis destruction. */
LE_API size_t le_analysis_node_count(const le_analysis_t* analysis);
LE_API const le_analysis_node_t* le_analysis_node_data(const le_analysis_t* analysis);
LE_API size_t le_analysis_child_count(const le_analysis_t* analysis);
LE_API const le_node_id_t* le_analysis_child_data(const le_analysis_t* analysis);
LE_API size_t le_analysis_feature_count(const le_analysis_t* analysis);
LE_API const le_feature_t* le_analysis_feature_data(const le_analysis_t* analysis);
LE_API size_t le_analysis_language_region_count(const le_analysis_t* analysis);
LE_API const le_language_region_t* le_analysis_language_region_data(const le_analysis_t* analysis);
/* Borrow the immutable UTF-8 source snapshot until analysis destruction. */
LE_API le_string_view_t le_analysis_text(const le_analysis_t* analysis);
/* Destroy an analysis and all nested arrays. NULL is accepted. */
LE_API void le_analysis_destroy(le_analysis_t* analysis);

/*
 * Generate reading signals from an immutable analysis. Inputs are borrowed for
 * the call; the caller owns out_signals.
 */
LE_API le_status_t le_generate_prefix_signals(le_runtime_t* runtime, const le_analysis_t* analysis,
                                              const le_prefix_model_config_t* config,
                                              le_signal_result_t** out_signals);
LE_API size_t le_signal_result_count(const le_signal_result_t* signals);
LE_API const le_reading_signal_t* le_signal_result_data(const le_signal_result_t* signals);
LE_API void le_signal_result_destroy(le_signal_result_t* signals);

/* Convert immutable reading signals into an owned, presentation-neutral plan. */
LE_API le_status_t le_generate_emphasis(le_runtime_t* runtime, const le_signal_result_t* signals,
                                        const le_presentation_config_t* config,
                                        le_result_t** out_result);

/*
 * Process UTF-8 text using generic Unicode analysis. options may be NULL.
 * Text is borrowed only for this call. On success, the caller owns out_result.
 */
LE_API le_status_t le_process(le_runtime_t* runtime, le_string_view_t text,
                              const le_process_options_t* options, le_result_t** out_result);

/* Borrow this thread's diagnostic until its next failing call; it may be truncated. */
LE_API le_string_view_t le_runtime_last_error(const le_runtime_t* runtime);

/* Return a static, null-terminated name for a status code. */
LE_API const char* le_status_string(le_status_t status);

/* Result accessors are thread-safe because results are immutable. */
LE_API size_t le_result_emphasis_count(const le_result_t* result);
LE_API const le_emphasis_t* le_result_emphasis_data(const le_result_t* result);

/* Destroy a result. NULL is accepted. */
LE_API void le_result_destroy(le_result_t* result);

#ifdef __cplusplus
}
#endif

#endif
