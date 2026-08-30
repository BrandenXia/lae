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

class BinaryBoldPolicy {
  public:
    explicit BinaryBoldPolicy(float strength) : strength_(strength) {}
    [[nodiscard]] std::vector<Emphasis> apply(const std::vector<ReadingSignal>& signals) const;

  private:
    float strength_;
};

} // namespace le::core

#endif
