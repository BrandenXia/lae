#ifndef LE_RUNTIME_CORE_LANGUAGE_HPP
#define LE_RUNTIME_CORE_LANGUAGE_HPP

#include "core/ir.hpp"
#include "core/text.hpp"

#include <string_view>

namespace le::core {

class LanguageProvider {
  public:
    virtual ~LanguageProvider() = default;
    [[nodiscard]] virtual bool supports(std::string_view language) const = 0;
    [[nodiscard]] virtual Analysis analyze(const Text& text, std::string_view language) const = 0;
};

} // namespace le::core

#endif
