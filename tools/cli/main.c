#include "le/api.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE* stream) {
    fputs("Usage: le-cli [--fixed N | --proportion P] [--strength S] [FILE]\n"
          "Reads UTF-8 from FILE or stdin and prints JSON byte spans.\n",
          stream);
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
    le_status_t status;
    int index;
    int exit_code = 1;

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
    status = le_process(runtime, (le_string_view_t){text, text_size}, &options, &result);
    if (status != LE_OK) {
        le_string_view_t detail = le_runtime_last_error(runtime);
        fprintf(stderr, "Processing failed: %s: %.*s\n", le_status_string(status), (int)detail.size,
                detail.data == NULL ? "" : detail.data);
        goto cleanup;
    }

    {
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
    le_result_destroy(result);
    le_runtime_destroy(runtime);
    free(text);
    if (input != stdin) {
        fclose(input);
    }
    return exit_code;
}
