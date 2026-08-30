#ifndef LE_RUNTIME_CORE_EMPHASIS_HPP
#define LE_RUNTIME_CORE_EMPHASIS_HPP

#include "core/reading.hpp"
#include "core/text.hpp"

#include <cstdint>
#include <vector>

namespace le::core {

struct Emphasis {
    TextSpan span;
    float strength;
    std::uint32_t style_class;
};

enum class PresentationPolicy : std::uint8_t {
    binary,
    variable_strength,
};

struct PresentationConfig {
    PresentationPolicy policy;
    float salience_threshold;
    float minimum_strength;
    float maximum_strength;
};

class BinaryBoldPolicy {
  public:
    BinaryBoldPolicy(float threshold, float strength)
        : threshold_(threshold), strength_(strength) {}
    [[nodiscard]] std::vector<Emphasis> apply(const std::vector<ReadingSignal>& signals) const;

  private:
    float threshold_;
    float strength_;
};

class VariableStrengthPolicy {
  public:
    VariableStrengthPolicy(float threshold, float minimum_strength, float maximum_strength)
        : threshold_(threshold), minimum_strength_(minimum_strength),
          maximum_strength_(maximum_strength) {}
    [[nodiscard]] std::vector<Emphasis> apply(const std::vector<ReadingSignal>& signals) const;

  private:
    float threshold_;
    float minimum_strength_;
    float maximum_strength_;
};

[[nodiscard]] std::vector<Emphasis> generate_emphasis(const std::vector<ReadingSignal>& signals,
                                                      const PresentationConfig& config);

} // namespace le::core

#endif
