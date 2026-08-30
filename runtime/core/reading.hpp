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

enum class ReadingModelKind : std::uint8_t {
    prefix,
    lexical_core,
};

class PrefixReadingModel {
  public:
    explicit PrefixReadingModel(PrefixModelConfig config) : config_(config) {}
    [[nodiscard]] std::vector<ReadingSignal> generate(const Text& text,
                                                      const Analysis& analysis) const;

  private:
    PrefixModelConfig config_;
};

class LexicalCoreReadingModel {
  public:
    [[nodiscard]] std::vector<ReadingSignal> generate(const Analysis& analysis) const;
};

} // namespace le::core

#endif
