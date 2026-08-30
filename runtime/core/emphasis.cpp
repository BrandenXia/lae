#include "core/emphasis.hpp"

#include <algorithm>

namespace le::core {
namespace {

void append_normalized(std::vector<Emphasis>& result, Emphasis emphasis) {
    if (!result.empty() && result.back().span.end() == emphasis.span.begin() &&
        result.back().strength == emphasis.strength &&
        result.back().style_class == emphasis.style_class) {
        result.back().span = TextSpan(result.back().span.begin(), emphasis.span.end());
        return;
    }
    result.push_back(emphasis);
}

} // namespace

std::vector<Emphasis> BinaryBoldPolicy::apply(const std::vector<ReadingSignal>& signals) const {
    std::vector<Emphasis> result;
    result.reserve(signals.size());
    for (const auto& signal : signals) {
        if (signal.fixation_salience <= 0.0F || signal.fixation_salience < threshold_ ||
            signal.span.empty()) {
            continue;
        }
        append_normalized(result, Emphasis{signal.span, strength_, 0});
    }
    return result;
}

std::vector<Emphasis>
VariableStrengthPolicy::apply(const std::vector<ReadingSignal>& signals) const {
    std::vector<Emphasis> result;
    result.reserve(signals.size());
    for (const auto& signal : signals) {
        if (signal.fixation_salience <= 0.0F || signal.fixation_salience < threshold_ ||
            signal.span.empty()) {
            continue;
        }
        const auto salience = std::clamp(signal.fixation_salience, 0.0F, 1.0F);
        const auto strength =
            minimum_strength_ + (maximum_strength_ - minimum_strength_) * salience;
        append_normalized(result, Emphasis{signal.span, strength, 0});
    }
    return result;
}

std::vector<Emphasis> generate_emphasis(const std::vector<ReadingSignal>& signals,
                                        const PresentationConfig& config) {
    if (config.policy == PresentationPolicy::variable_strength) {
        return VariableStrengthPolicy(config.salience_threshold, config.minimum_strength,
                                      config.maximum_strength)
            .apply(signals);
    }
    return BinaryBoldPolicy(config.salience_threshold, config.maximum_strength).apply(signals);
}

} // namespace le::core
