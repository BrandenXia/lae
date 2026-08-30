#ifndef LE_RUNTIME_CORE_LANGUAGE_HPP
#define LE_RUNTIME_CORE_LANGUAGE_HPP

#include "core/ir.hpp"
#include "core/text.hpp"

#include <string_view>

namespace le::core {

class LanguageProvider {
  public:
    virtual ~LanguageProvider() = default;
    [[nodiscard]] virtual Analysis analyze(const Text& text, std::string_view language) const = 0;
};

class GenericLanguageProvider final : public LanguageProvider {
  public:
    [[nodiscard]] Analysis analyze(const Text& text, std::string_view language) const override;
};

} // namespace le::core

#endif
