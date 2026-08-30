#include "le/api.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE* stream) {
    fputs("Usage: le-cli [--language TAG] [--dump-analysis | --dump-signals]\n"
          "              [--model prefix|lexical-core]\n"
          "              [--fixed N | --proportion P] [--policy binary|variable]\n"
          "              [--threshold S] [--min-strength S] [--strength S] [FILE]\n"
          "Reads UTF-8 from FILE or stdin and prints JSON for the selected stage.\n",
          stream);
}

static const char* node_kind_name(le_node_kind_t kind) {
    switch (kind) {
    case LE_NODE_DOCUMENT:
        return "document";
    case LE_NODE_BLOCK:
        return "block";
    case LE_NODE_PARAGRAPH:
        return "paragraph";
    case LE_NODE_SENTENCE:
        return "sentence";
    case LE_NODE_UNIT:
        return "unit";
    case LE_NODE_SUBUNIT:
        return "subunit";
    default:
        return "unknown";
    }
}

static void print_analysis(const le_analysis_t* analysis) {
    size_t node_count = le_analysis_node_count(analysis);
    const le_analysis_node_t* nodes = le_analysis_node_data(analysis);
    const le_node_id_t* children = le_analysis_child_data(analysis);
    const le_feature_t* features = le_analysis_feature_data(analysis);
    size_t region_count = le_analysis_language_region_count(analysis);
    const le_language_region_t* regions = le_analysis_language_region_data(analysis);
    size_t node_index;
    size_t region_index;

    fputs("{\n  \"nodes\":[\n", stdout);
    for (node_index = 0; node_index < node_count; ++node_index) {
        const le_analysis_node_t* node = &nodes[node_index];
        uint32_t item;
        printf("    {\"id\":%u,\"kind\":\"%s\",\"begin\":%llu,\"end\":%llu,\"children\":[",
               node->id, node_kind_name(node->kind), (unsigned long long)node->span.begin,
               (unsigned long long)node->span.end);
        for (item = 0; item < node->child_count; ++item) {
            printf("%s%u", item == 0 ? "" : ",", children[node->first_child + item]);
        }
        fputs("],\"features\":[", stdout);
        for (item = 0; item < node->feature_count; ++item) {
            const le_feature_t* feature = &features[node->first_feature + item];
            printf("%s{\"id\":%u,\"value\":%.3f}", item == 0 ? "" : ",", feature->id,
                   (double)feature->value);
        }
        printf("]}%s\n", node_index + 1 == node_count ? "" : ",");
    }
    fputs("  ],\n  \"language_regions\":[\n", stdout);
    for (region_index = 0; region_index < region_count; ++region_index) {
        const le_language_region_t* region = &regions[region_index];
        printf("    {\"begin\":%llu,\"end\":%llu,\"language\":\"%.*s\",\"confidence\":%.3f}%s\n",
               (unsigned long long)region->span.begin, (unsigned long long)region->span.end,
               (int)region->language.size, region->language.data, (double)region->confidence,
               region_index + 1 == region_count ? "" : ",");
    }
    fputs("  ]\n}\n", stdout);
}

static void print_signals(const le_signal_result_t* signals) {
    size_t count = le_signal_result_count(signals);
    const le_reading_signal_t* items = le_signal_result_data(signals);
    size_t index;
    fputs("[\n", stdout);
    for (index = 0; index < count; ++index) {
        printf("  {\"begin\":%llu,\"end\":%llu,\"fixation_salience\":%.3f,"
               "\"lexical_salience\":%.3f,\"reading_difficulty\":%.3f}%s\n",
               (unsigned long long)items[index].span.begin,
               (unsigned long long)items[index].span.end, (double)items[index].fixation_salience,
               (double)items[index].lexical_salience, (double)items[index].reading_difficulty,
               index + 1 == count ? "" : ",");
    }
    fputs("]\n", stdout);
}

static int parse_float(const char* text, float* value) {
    char* end = NULL;
    errno = 0;
    *value = strtof(text, &end);
    return errno == 0 && end != text && *end == '\0';
}

static int parse_uint32(const char* text, uint32_t* value) {
    char* end = NULL;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int read_all(FILE* input, char** data, size_t* size) {
    size_t capacity = 4096;
    *size = 0;
    *data = (char*)malloc(capacity);
    if (*data == NULL) {
        return 0;
    }
    for (;;) {
        size_t available = capacity - *size;
        size_t count = fread(*data + *size, 1, available, input);
        *size += count;
        if (count < available) {
            if (ferror(input)) {
                free(*data);
                *data = NULL;
                return 0;
            }
            return 1;
        }
        if (capacity > SIZE_MAX / 2) {
            free(*data);
            *data = NULL;
            return 0;
        }
        capacity *= 2;
        {
            char* grown = (char*)realloc(*data, capacity);
            if (grown == NULL) {
                free(*data);
                *data = NULL;
                return 0;
            }
            *data = grown;
        }
    }
}

int main(int argc, char** argv) {
    le_process_options_t options;
    const char* path = NULL;
    FILE* input = stdin;
    char* text = NULL;
    size_t text_size = 0;
    le_runtime_t* runtime = NULL;
    le_result_t* result = NULL;
    le_analysis_t* analysis = NULL;
    le_signal_result_t* signals = NULL;
    le_status_t status;
    int index;
    int exit_code = 1;
    int dump_analysis = 0;
    int dump_signals = 0;

    le_process_options_init(&options);
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--fixed") == 0 && index + 1 < argc) {
            options.prefix_strategy = LE_PREFIX_FIXED;
            if (!parse_uint32(argv[++index], &options.fixed_graphemes)) {
                fputs("Invalid fixed grapheme count.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--proportion") == 0 && index + 1 < argc) {
            options.prefix_strategy = LE_PREFIX_PROPORTIONAL;
            if (!parse_float(argv[++index], &options.prefix_proportion)) {
                fputs("Invalid prefix proportion.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--strength") == 0 && index + 1 < argc) {
            if (!parse_float(argv[++index], &options.emphasis_strength)) {
                fputs("Invalid emphasis strength.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--min-strength") == 0 && index + 1 < argc) {
            if (!parse_float(argv[++index], &options.minimum_emphasis_strength)) {
                fputs("Invalid minimum emphasis strength.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--threshold") == 0 && index + 1 < argc) {
            if (!parse_float(argv[++index], &options.salience_threshold)) {
                fputs("Invalid salience threshold.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--policy") == 0 && index + 1 < argc) {
            const char* policy = argv[++index];
            if (strcmp(policy, "binary") == 0) {
                options.presentation_policy = LE_POLICY_BINARY;
            } else if (strcmp(policy, "variable") == 0) {
                options.presentation_policy = LE_POLICY_VARIABLE_STRENGTH;
            } else {
                fputs("Policy must be binary or variable.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--model") == 0 && index + 1 < argc) {
            const char* model = argv[++index];
            if (strcmp(model, "prefix") == 0) {
                options.reading_model = LE_READING_MODEL_PREFIX;
            } else if (strcmp(model, "lexical-core") == 0) {
                options.reading_model = LE_READING_MODEL_LEXICAL_CORE;
            } else {
                fputs("Model must be prefix or lexical-core.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--language") == 0 && index + 1 < argc) {
            const char* language = argv[++index];
            options.language = (le_string_view_t){language, strlen(language)};
        } else if (strcmp(argv[index], "--dump-analysis") == 0) {
            dump_analysis = 1;
        } else if (strcmp(argv[index], "--dump-signals") == 0) {
            dump_signals = 1;
        } else if (strcmp(argv[index], "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (argv[index][0] == '-' || path != NULL) {
            usage(stderr);
            return 2;
        } else {
            path = argv[index];
        }
    }

    if (dump_analysis && dump_signals) {
        fputs("Choose only one dump stage.\n", stderr);
        return 2;
    }

    if (path != NULL) {
        input = fopen(path, "rb");
        if (input == NULL) {
            fprintf(stderr, "Could not open %s: %s\n", path, strerror(errno));
            return 1;
        }
    }
    if (!read_all(input, &text, &text_size)) {
        fputs("Could not read input.\n", stderr);
        goto cleanup;
    }

    status = le_runtime_create(NULL, &runtime);
    if (status != LE_OK) {
        fprintf(stderr, "Runtime creation failed: %s\n", le_status_string(status));
        goto cleanup;
    }
    if (dump_analysis || dump_signals) {
        status =
            le_analyze(runtime, (le_string_view_t){text, text_size}, options.language, &analysis);
    } else {
        status = le_process(runtime, (le_string_view_t){text, text_size}, &options, &result);
    }
    if (status != LE_OK) {
        le_string_view_t detail = le_runtime_last_error(runtime);
        fprintf(stderr, "%s failed: %s: %.*s\n",
                dump_analysis || dump_signals ? "Analysis" : "Processing", le_status_string(status),
                (int)detail.size, detail.data == NULL ? "" : detail.data);
        goto cleanup;
    }

    if (dump_signals) {
        if (options.reading_model == LE_READING_MODEL_LEXICAL_CORE) {
            status = le_generate_lexical_core_signals(runtime, analysis, &signals);
        } else {
            le_prefix_model_config_t model_config;
            le_prefix_model_config_init(&model_config);
            model_config.strategy = options.prefix_strategy;
            model_config.fixed_graphemes = options.fixed_graphemes;
            model_config.proportion = options.prefix_proportion;
            status = le_generate_prefix_signals(runtime, analysis, &model_config, &signals);
        }
        if (status != LE_OK) {
            le_string_view_t detail = le_runtime_last_error(runtime);
            fprintf(stderr, "Signal generation failed: %s: %.*s\n", le_status_string(status),
                    (int)detail.size, detail.data == NULL ? "" : detail.data);
            goto cleanup;
        }
    }

    if (dump_analysis) {
        print_analysis(analysis);
    } else if (dump_signals) {
        print_signals(signals);
    } else {
        size_t count = le_result_emphasis_count(result);
        const le_emphasis_t* items = le_result_emphasis_data(result);
        size_t item;
        fputs("[\n", stdout);
        for (item = 0; item < count; ++item) {
            printf("  {\"begin\":%llu,\"end\":%llu,\"strength\":%.3f,\"style_class\":%u}%s\n",
                   (unsigned long long)items[item].span.begin,
                   (unsigned long long)items[item].span.end, (double)items[item].strength,
                   items[item].style_class, item + 1 == count ? "" : ",");
        }
        fputs("]\n", stdout);
    }
    exit_code = 0;

cleanup:
    le_signal_result_destroy(signals);
    le_analysis_destroy(analysis);
    le_result_destroy(result);
    le_runtime_destroy(runtime);
    free(text);
    if (input != stdin) {
        fclose(input);
    }
    return exit_code;
}
