#ifndef LE_RUNTIME_CORE_READING_HPP
#define LE_RUNTIME_CORE_READING_HPP

#include "core/ir.hpp"
#include "core/text.hpp"

#include <cstdint>
#include <vector>

namespace le::core {

struct ReadingSignal {
    TextSpan span;
    float fixation_salience;
    float lexical_salience;
    float reading_difficulty;
};

enum class PrefixStrategy : std::uint8_t {
    proportional,
    fixed,
};

struct PrefixModelConfig {
    PrefixStrategy strategy;
    std::uint32_t fixed_graphemes;
    float proportion;
};

class PrefixReadingModel {
  public:
    explicit PrefixReadingModel(PrefixModelConfig config) : config_(config) {}
    [[nodiscard]] std::vector<ReadingSignal> generate(const Text& text,
                                                      const Analysis& analysis) const;

  private:
    PrefixModelConfig config_;
};

} // namespace le::core

#endif
