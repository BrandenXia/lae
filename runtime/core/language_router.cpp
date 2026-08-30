#include "core/language_router.hpp"

#include "providers/english/english_provider.hpp"
#include "providers/generic/generic_provider.hpp"

namespace le::core {

Analysis LanguageRouter::analyze(const Text& text, std::string_view language) const {
    const EnglishLanguageProvider english;
    if (english.supports(language)) {
        return english.analyze(text, language);
    }
    const GenericLanguageProvider generic;
    return generic.analyze(text, language);
}

} // namespace le::core
