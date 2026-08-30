#include "core/emphasis.hpp"

namespace le::core {

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

} // namespace le::core
