#include "core/pipeline.hpp"

#include "core/emphasis.hpp"
#include "core/ir.hpp"
#include "core/language_router.hpp"
#include "core/reading.hpp"

namespace le::core {

Analysis analyze(const Text& text, std::string_view language) {
    const LanguageRouter router;
    auto analysis = router.analyze(text, language);
    validate_analysis(text, analysis);
    return analysis;
}

std::vector<Emphasis> process(const Text& text, const PipelineOptions& options) {
    const auto analysis = analyze(text, options.language);
    std::vector<ReadingSignal> signals;
    if (options.reading_model == ReadingModelKind::lexical_core) {
        signals = LexicalCoreReadingModel().generate(analysis);
    } else {
        signals = PrefixReadingModel(options.prefix).generate(text, analysis);
    }
    return generate_emphasis(signals, options.presentation);
}

} // namespace le::core
