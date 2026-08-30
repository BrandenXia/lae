#include "core/ir.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace le::core {
namespace {

bool contains(TextSpan parent, TextSpan child) {
    return parent.begin() <= child.begin() && child.end() <= parent.end();
}

} // namespace

void validate_analysis(const Text& text, const Analysis& analysis) {
    if (analysis.nodes.empty()) {
        throw InvalidAnalysis("analysis has no document root");
    }
    const auto full_text = TextSpan(ByteOffset(0), ByteOffset(text.bytes().size()));
    const auto& root = analysis.nodes.front();
    if (root.id.value() != 0 || root.kind != NodeKind::document ||
        root.span.begin() != full_text.begin() || root.span.end() != full_text.end()) {
        throw InvalidAnalysis("analysis root is not document node 0 over the full text");
    }

    std::vector<std::uint32_t> parent_counts(analysis.nodes.size(), 0);
    for (std::size_t index = 0; index < analysis.nodes.size(); ++index) {
        const auto& node = analysis.nodes[index];
        if (node.id.value() != index) {
            throw InvalidAnalysis("node identifiers are not dense and ordered");
        }
        if (!text.is_valid_span(node.span, SpanBoundary::grapheme)) {
            throw InvalidAnalysis("node span is outside text or splits a grapheme");
        }

        std::vector<FeatureId> feature_ids;
        feature_ids.reserve(node.features.size());
        for (const auto& feature : node.features) {
            if (!std::isfinite(feature.value)) {
                throw InvalidAnalysis("node feature value is not finite");
            }
            if (std::ranges::find(feature_ids, feature.id) != feature_ids.end()) {
                throw InvalidAnalysis("node contains a duplicate feature identifier");
            }
            feature_ids.push_back(feature.id);
        }

        ByteOffset previous_end = node.span.begin();
        for (const auto child_id : node.children) {
            if (child_id.value() >= analysis.nodes.size() || child_id.value() == 0) {
                throw InvalidAnalysis("node child identifier is invalid");
            }
            const auto& child = analysis.nodes[child_id.value()];
            if (!contains(node.span, child.span)) {
                throw InvalidAnalysis("child span is not contained by its parent");
            }
            if (child.span.begin() < previous_end) {
                throw InvalidAnalysis("sibling spans overlap or are out of order");
            }
            previous_end = child.span.end();
            ++parent_counts[child_id.value()];
        }
    }
    for (std::size_t index = 1; index < parent_counts.size(); ++index) {
        if (parent_counts[index] != 1) {
            throw InvalidAnalysis("every non-root node must have exactly one parent");
        }
    }

    std::vector<bool> reachable(analysis.nodes.size(), false);
    std::vector<NodeId> pending{NodeId(0)};
    reachable.front() = true;
    std::size_t reachable_count = 0;
    while (!pending.empty()) {
        const auto node_id = pending.back();
        pending.pop_back();
        ++reachable_count;
        for (const auto child_id : analysis.nodes[node_id.value()].children) {
            if (reachable[child_id.value()]) {
                throw InvalidAnalysis("analysis contains a child cycle");
            }
            reachable[child_id.value()] = true;
            pending.push_back(child_id);
        }
    }
    if (reachable_count != analysis.nodes.size()) {
        throw InvalidAnalysis("analysis contains nodes disconnected from the document root");
    }

    ByteOffset previous_region_end(0);
    for (const auto& region : analysis.language_regions) {
        if (!text.is_valid_span(region.span, SpanBoundary::grapheme)) {
            throw InvalidAnalysis("language region span is invalid");
        }
        if (region.span.begin() < previous_region_end) {
            throw InvalidAnalysis("language regions overlap or are out of order");
        }
        if (region.language.empty()) {
            throw InvalidAnalysis("language region has an empty language tag");
        }
        if (!std::isfinite(region.confidence) || region.confidence < 0.0F ||
            region.confidence > 1.0F) {
            throw InvalidAnalysis("language region confidence is not in [0, 1]");
        }
        previous_region_end = region.span.end();
    }
}

} // namespace le::core
