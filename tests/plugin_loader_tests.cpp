#include "le/api.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    check(argc == 2, "plugin path argument is present");
    if (argc != 2) {
        return EXIT_FAILURE;
    }

    le_runtime_t* runtime = nullptr;
    check(le_runtime_create(nullptr, &runtime) == LE_OK, "runtime creation succeeds");
    check(le_runtime_dynamic_providers_enabled() == 1, "dynamic provider loading is enabled");
    check(le_runtime_load_provider(runtime, le_string_view_t{"/does/not/exist", 15}) ==
              LE_ERROR_PLUGIN_FAILURE,
          "missing dynamic module reports a plugin failure");
    const std::string_view path(argv[1]);
    check(le_runtime_load_provider(runtime, le_string_view_t{path.data(), path.size()}) == LE_OK,
          "dynamic provider loads");
    check(le_runtime_provider_count(runtime) == 1, "dynamic provider is discovered");
    const auto name = le_runtime_provider_name_at(runtime, 0);
    check(std::string_view(name.data, name.size) == "test-dynamic", "provider name is exposed");
    check(le_runtime_load_provider(runtime, le_string_view_t{path.data(), path.size()}) ==
              LE_ERROR_PLUGIN_INCOMPATIBLE,
          "duplicate dynamic provider is rejected");
    check(le_runtime_provider_count(runtime) == 1, "duplicate module does not change discovery");

    le_analysis_t* analysis = nullptr;
    check(le_analyze(runtime, le_string_view_t{"module", 6}, le_string_view_t{"xd", 2},
                     &analysis) == LE_OK,
          "dynamic provider analyzes supported language");
    check(le_analysis_node_count(analysis) == 2, "dynamic provider graph is retained");
    check(le_analysis_feature_count(analysis) == 1, "dynamic provider feature is retained");
    check(le_analysis_feature_data(analysis)[0].id == LE_FEATURE_RANGE_VENDOR_BEGIN,
          "dynamic provider vendor feature is retained");
    le_analysis_destroy(analysis);
    le_runtime_destroy(runtime);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All dynamic provider tests passed\n";
    return EXIT_SUCCESS;
}
