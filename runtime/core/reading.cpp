#include "core/reading.hpp"

#include <algorithm>
#include <cmath>

namespace le::core {

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

        for (std::size_t index = 0; index < count; ++index) {
            const auto position =
                count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1);
            const auto salience = 1.0F - 0.5F * position;
            signals.push_back(ReadingSignal{graphemes[index]->span, salience, 0.0F, 0.0F});
        }
    }
    return signals;
}

} // namespace le::core
