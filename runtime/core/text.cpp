#include "core/text.hpp"

#include <algorithm>
#include <stdexcept>

#include <utf8proc.h>

namespace le::core {

Text::Text(std::string_view bytes) : bytes_(bytes) {
    std::size_t offset = 0;
    while (offset < bytes_.size()) {
        utf8proc_int32_t code_point = 0;
        const auto remaining = bytes_.size() - offset;
        const auto length =
            utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(bytes_.data() + offset),
                             static_cast<utf8proc_ssize_t>(remaining), &code_point);
        if (length <= 0) {
            throw InvalidUtf8("input is not valid UTF-8");
        }
        code_points_.push_back(
            CodePoint{static_cast<std::int32_t>(code_point), ByteOffset(offset)});
        offset += static_cast<std::size_t>(length);
    }

    if (code_points_.empty()) {
        return;
    }

    std::size_t grapheme_start = 0;
    utf8proc_int32_t state = 0;
    for (std::size_t index = 1; index < code_points_.size(); ++index) {
        const auto breaks = utf8proc_grapheme_break_stateful(code_points_[index - 1].value,
                                                             code_points_[index].value, &state);
        if (breaks != 0) {
            graphemes_.push_back(
                Grapheme{TextSpan(code_points_[grapheme_start].begin, code_points_[index].begin),
                         code_points_[grapheme_start].value});
            grapheme_start = index;
        }
    }

    graphemes_.push_back(
        Grapheme{TextSpan(code_points_[grapheme_start].begin, ByteOffset(bytes_.size())),
                 code_points_[grapheme_start].value});
}

bool Text::is_byte_boundary(ByteOffset offset) const {
    if (offset.value() == bytes_.size()) {
        return true;
    }
    return std::ranges::any_of(
        code_points_, [offset](const CodePoint& code_point) { return code_point.begin == offset; });
}

bool Text::is_grapheme_boundary(ByteOffset offset) const {
    if (offset.value() == bytes_.size()) {
        return true;
    }
    return std::ranges::any_of(
        graphemes_, [offset](const Grapheme& grapheme) { return grapheme.span.begin() == offset; });
}

bool Text::is_valid_span(TextSpan span, SpanBoundary boundary) const {
    if (span.end().value() > bytes_.size() || !is_byte_boundary(span.begin()) ||
        !is_byte_boundary(span.end())) {
        return false;
    }
    return boundary == SpanBoundary::code_point ||
           (is_grapheme_boundary(span.begin()) && is_grapheme_boundary(span.end()));
}

std::vector<const Grapheme*> Text::graphemes_in(TextSpan span) const {
    if (!is_valid_span(span, SpanBoundary::grapheme)) {
        throw std::logic_error("span does not satisfy text grapheme boundaries");
    }
    std::vector<const Grapheme*> result;
    for (const auto& grapheme : graphemes_) {
        if (grapheme.span.begin() >= span.begin() && grapheme.span.end() <= span.end()) {
            result.push_back(&grapheme);
        }
    }
    return result;
}

} // namespace le::core
