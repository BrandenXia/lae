#include "le/api.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<le_string_view_t>);
static_assert(std::is_standard_layout_v<le_text_span_t>);
static_assert(std::is_standard_layout_v<le_emphasis_t>);
static_assert(sizeof(le_status_t) == sizeof(std::int32_t));
static_assert(offsetof(le_text_span_t, begin) == 0);
static_assert(LE_ABI_VERSION_MAJOR == 1);

int main() {
    le_runtime_t* runtime = nullptr;
    if (le_runtime_create(nullptr, &runtime) != LE_OK) {
        return 1;
    }
    le_runtime_destroy(runtime);
    return 0;
}
