#include "core/pipeline.hpp"

#include "core/emphasis.hpp"
#include "core/ir.hpp"
#include "core/language.hpp"
#include "core/reading.hpp"

namespace le::core {

Analysis analyze(const Text& text, std::string_view language) {
    const GenericLanguageProvider provider;
    auto analysis = provider.analyze(text, language);
    validate_analysis(text, analysis);
    return analysis;
}

std::vector<Emphasis> process(const Text& text, const PipelineOptions& options) {
    const auto analysis = analyze(text, options.language);
    const PrefixReadingModel model(options.prefix);
    const auto signals = model.generate(text, analysis);
    return generate_emphasis(signals, options.presentation);
}

} // namespace le::core
