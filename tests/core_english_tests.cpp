#include "core/ir.hpp"
#include "core/text.hpp"
#include "providers/english/english_provider.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct ExpectedSubunit {
    std::string_view text;
    le::core::FeatureId feature;
};

} // namespace

int main() {
    using namespace le::core;

    const EnglishLanguageProvider provider;
    check(provider.supports("en"), "provider supports en");
    check(provider.supports("en-US"), "provider supports English regional tags");
    check(provider.supports("EN"), "provider language matching is case-insensitive");
    check(!provider.supports("eng"), "provider rejects unrelated language prefixes");
    check(!provider.supports("zh"), "provider rejects non-English tags");

    const Text text("Unbelievable readers were reading. Quickly replayed!");
    const auto analysis = provider.analyze(text, "en-US");
    validate_analysis(text, analysis);

    std::size_t sentences = 0;
    std::size_t units = 0;
    std::vector<ExpectedSubunit> subunits;
    for (const auto& node : analysis.nodes) {
        if (node.kind == NodeKind::sentence) {
            ++sentences;
        } else if (node.kind == NodeKind::unit) {
            ++units;
        } else if (node.kind == NodeKind::subunit) {
            const auto begin = static_cast<std::size_t>(node.span.begin().value());
            const auto size =
                static_cast<std::size_t>(node.span.end().value() - node.span.begin().value());
            check(node.features.size() == 1, "subunit has one morphology feature");
            if (!node.features.empty()) {
                subunits.push_back({text.bytes().substr(begin, size), node.features[0].id});
            }
        }
    }
    check(sentences == 2, "terminal punctuation creates two sentence nodes");
    check(units == 6, "letter runs create six unit nodes");

    const std::vector<ExpectedSubunit> expected{
        {"Un", feature_derivational_affix},   {"believ", feature_lexical_core},
        {"able", feature_derivational_affix}, {"reader", feature_lexical_core},
        {"s", feature_grammatical_affix},     {"were", feature_lexical_core},
        {"read", feature_lexical_core},       {"ing", feature_grammatical_affix},
        {"Quick", feature_lexical_core},      {"ly", feature_derivational_affix},
        {"re", feature_derivational_affix},   {"play", feature_lexical_core},
        {"ed", feature_grammatical_affix},
    };
    check(subunits.size() == expected.size(), "morphology produces expected subunit count");
    const auto count = std::min(subunits.size(), expected.size());
    for (std::size_t index = 0; index < count; ++index) {
        check(subunits[index].text == expected[index].text, "subunit text matches rule output");
        check(subunits[index].feature == expected[index].feature,
              "subunit feature matches rule output");
    }

    const Text unicode("Éclair can’t");
    const auto unicode_analysis = provider.analyze(unicode, "en");
    validate_analysis(unicode, unicode_analysis);
    check(unicode_analysis.nodes.size() >= 5, "Unicode letters and curly apostrophes remain valid");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core English provider tests passed\n";
    return EXIT_SUCCESS;
}
