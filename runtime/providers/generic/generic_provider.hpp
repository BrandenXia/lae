#ifndef LE_RUNTIME_PROVIDERS_GENERIC_PROVIDER_HPP
#define LE_RUNTIME_PROVIDERS_GENERIC_PROVIDER_HPP

#include "core/language.hpp"

namespace le::core {

class GenericLanguageProvider final : public LanguageProvider {
  public:
    [[nodiscard]] bool supports(std::string_view language) const override;
    [[nodiscard]] Analysis analyze(const Text& text, std::string_view language) const override;
};

} // namespace le::core

#endif
