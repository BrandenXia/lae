#include "core/pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

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

Analysis GenericLanguageProvider::analyze(const Text& text, std::string_view language) const {
    Analysis result;
    result.nodes.push_back(Node{NodeId(0),
                                TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())),
                                NodeKind::document,
                                {},
                                {}});
    result.language_regions.push_back(LanguageRegion{
        TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())),
        language.empty() ? std::string_view("und") : language, language.empty() ? 0.0F : 1.0F});

    std::optional<ByteOffset> unit_begin;
    auto finish_unit = [&](ByteOffset end) {
        if (!unit_begin.has_value()) {
            return;
        }
        const auto id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
        result.nodes.push_back(Node{id, TextSpan(*unit_begin, end), NodeKind::unit, {}, {}});
        result.nodes.front().children.push_back(id);
        unit_begin.reset();
    };

    for (const auto& grapheme : text.graphemes()) {
        if (is_unit_separator(grapheme.first_code_point)) {
            finish_unit(grapheme.span.begin());
        } else if (!unit_begin.has_value()) {
            unit_begin = grapheme.span.begin();
        }
    }
    finish_unit(ByteOffset(text.bytes().size()));
    return result;
}

std::vector<ReadingSignal> PrefixReadingModel::generate(const Text& text,
                                                        const Analysis& analysis) const {
    std::vector<ReadingSignal> signals;
    for (const auto& node : analysis.nodes) {
        if (node.kind != NodeKind::unit) {
            continue;
        }
        const auto graphemes = text.graphemes_in(node.span);
        if (graphemes.empty()) {
            continue;
        }

        std::size_t count = 0;
        if (config_.strategy == PrefixStrategy::fixed) {
            count = std::min<std::size_t>(config_.fixed_graphemes, graphemes.size());
        } else if (config_.proportion > 0.0F) {
            count = static_cast<std::size_t>(
                std::ceil(static_cast<double>(graphemes.size()) * config_.proportion));
            count = std::clamp<std::size_t>(count, 1, graphemes.size());
        }
        if (count == 0) {
            continue;
        }

        signals.push_back(ReadingSignal{
            TextSpan(graphemes.front()->span.begin(), graphemes[count - 1]->span.end()), 1.0F, 0.0F,
            0.0F});
    }
    return signals;
}

std::vector<Emphasis> BinaryBoldPolicy::apply(const std::vector<ReadingSignal>& signals) const {
    std::vector<Emphasis> result;
    result.reserve(signals.size());
    for (const auto& signal : signals) {
        if (signal.fixation_salience <= 0.0F || signal.span.empty()) {
            continue;
        }
        result.push_back(Emphasis{signal.span, strength_, 0});
    }
    return result;
}

std::vector<Emphasis> process(const Text& text, const PipelineOptions& options) {
    const GenericLanguageProvider provider;
    const auto analysis = provider.analyze(text, options.language);
    const PrefixReadingModel model(options.prefix);
    const auto signals = model.generate(text, analysis);
    const BinaryBoldPolicy policy(options.emphasis_strength);
    return policy.apply(signals);
}

} // namespace le::core
