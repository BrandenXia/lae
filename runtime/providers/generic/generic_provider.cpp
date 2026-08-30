#include "providers/generic/generic_provider.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include <utf8proc.h>

namespace le::core {
namespace {

bool is_unit_separator(std::int32_t code_point) {
    switch (utf8proc_category(code_point)) {
    case UTF8PROC_CATEGORY_CC:
    case UTF8PROC_CATEGORY_CF:
    case UTF8PROC_CATEGORY_ZS:
    case UTF8PROC_CATEGORY_ZL:
    case UTF8PROC_CATEGORY_ZP:
    case UTF8PROC_CATEGORY_PC:
    case UTF8PROC_CATEGORY_PD:
    case UTF8PROC_CATEGORY_PS:
    case UTF8PROC_CATEGORY_PE:
    case UTF8PROC_CATEGORY_PI:
    case UTF8PROC_CATEGORY_PF:
    case UTF8PROC_CATEGORY_PO:
        return true;
    default:
        return false;
    }
}

} // namespace

bool GenericLanguageProvider::supports(std::string_view) const { return true; }

Analysis GenericLanguageProvider::analyze(const Text& text, std::string_view language) const {
    Analysis result;
    result.nodes.push_back(
        Node{NodeId(0),
             TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())),
             NodeKind::document,
             {},
             {Feature{feature_grapheme_count, static_cast<float>(text.graphemes().size())}}});
    result.language_regions.push_back(
        LanguageRegion{TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())),
                       std::string(language.empty() ? std::string_view("und") : language),
                       language.empty() || language == "und" ? 0.0F : 1.0F});

    std::optional<ByteOffset> unit_begin;
    std::uint32_t unit_graphemes = 0;
    auto finish_unit = [&](ByteOffset end) {
        if (!unit_begin.has_value()) {
            return;
        }
        const auto id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
        result.nodes.push_back(
            Node{id,
                 TextSpan(*unit_begin, end),
                 NodeKind::unit,
                 {},
                 {Feature{feature_boundary_strength, 1.0F},
                  Feature{feature_grapheme_count, static_cast<float>(unit_graphemes)}}});
        result.nodes.front().children.push_back(id);
        unit_begin.reset();
        unit_graphemes = 0;
    };

    for (const auto& grapheme : text.graphemes()) {
        if (is_unit_separator(grapheme.first_code_point)) {
            finish_unit(grapheme.span.begin());
        } else {
            if (!unit_begin.has_value()) {
                unit_begin = grapheme.span.begin();
            }
            ++unit_graphemes;
        }
    }
    finish_unit(ByteOffset(text.bytes().size()));
    return result;
}

} // namespace le::core
