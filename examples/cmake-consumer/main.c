#include "le/api.h"

#include <stddef.h>

int main(void) {
    static const char text[] = "packaged consumer";
    le_runtime_t* runtime = NULL;
    le_result_t* result = NULL;
    int exit_code = 0;
    if (le_runtime_create(NULL, &runtime) != LE_OK)
        return 1;
    if (le_process(runtime, (le_string_view_t){text, sizeof(text) - 1}, NULL, &result) != LE_OK)
        exit_code = 2;
    else if (le_result_emphasis_count(result) != 2)
        exit_code = 3;
    le_result_destroy(result);
    le_runtime_destroy(runtime);
    return exit_code;
}
