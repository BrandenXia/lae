#include "core/text.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
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

int main() {
    using le::core::ByteOffset;
    using le::core::SpanBoundary;
    using le::core::Text;
    using le::core::TextSpan;

    const Text ascii("abc");
    check(ascii.is_valid_span(TextSpan(ByteOffset(0), ByteOffset(0)), SpanBoundary::grapheme),
          "empty span is valid");
    check(ascii.is_valid_span(TextSpan(ByteOffset(0), ByteOffset(3)), SpanBoundary::grapheme),
          "full-document span is valid");
    check(!ascii.is_valid_span(TextSpan(ByteOffset(0), ByteOffset(4)), SpanBoundary::grapheme),
          "out-of-range span is invalid");

    bool reversed_rejected = false;
    try {
        static_cast<void>(TextSpan(ByteOffset(2), ByteOffset(1)));
    } catch (const std::logic_error&) {
        reversed_rejected = true;
    }
    check(reversed_rejected, "reversed span is rejected");

    const Text composed("é");
    check(!composed.is_valid_span(TextSpan(ByteOffset(1), ByteOffset(2)), SpanBoundary::code_point),
          "UTF-8 continuation-byte boundary is invalid");

    const Text combining("e\u0301");
    const TextSpan split_cluster(ByteOffset(1), ByteOffset(3));
    check(combining.is_valid_span(split_cluster, SpanBoundary::code_point),
          "combining mark begins at a code-point boundary");
    check(!combining.is_valid_span(split_cluster, SpanBoundary::grapheme),
          "combining cluster cannot be split by emphasis");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core text tests passed\n";
    return EXIT_SUCCESS;
}
