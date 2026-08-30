#ifndef LE_RUNTIME_CORE_LANGUAGE_ROUTER_HPP
#define LE_RUNTIME_CORE_LANGUAGE_ROUTER_HPP

#include "core/ir.hpp"
#include "core/text.hpp"

#include <string_view>

namespace le::core {

class LanguageRouter {
  public:
    [[nodiscard]] Analysis analyze(const Text& text, std::string_view language) const;
};

} // namespace le::core

#endif
