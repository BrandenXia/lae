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

std::string_view slice(const le::core::Text& text, const le::core::Node& node) {
    const auto begin = static_cast<std::size_t>(node.span.begin().value());
    const auto size = static_cast<std::size_t>(node.span.end().value() - node.span.begin().value());
    return text.bytes().substr(begin, size);
}

bool has_feature(const le::core::Node& node, le::core::FeatureId id, float value = 1.0F) {
    return std::ranges::any_of(node.features, [=](const le::core::Feature& feature) {
        return feature.id == id && feature.value == value;
    });
}

bool has_feature_id(const le::core::Node& node, le::core::FeatureId id) {
    return std::ranges::any_of(node.features,
                               [=](const le::core::Feature& feature) { return feature.id == id; });
}

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
    std::vector<FeatureId> semantic_features;
    for (const auto& node : analysis.nodes) {
        if (node.kind == NodeKind::sentence) {
            ++sentences;
        } else if (node.kind == NodeKind::unit) {
            ++units;
            check(has_feature(node, feature_segmentation_confidence),
                  "English tokenization has full segmentation confidence");
            check(has_feature_id(node, feature_unit_position) &&
                      has_feature_id(node, feature_sentence_unit_count),
                  "English units expose sentence-local structural features");
            semantic_features.push_back(has_feature(node, feature_function_unit)
                                            ? feature_function_unit
                                            : feature_content_unit);
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
    check(semantic_features == std::vector<FeatureId>{feature_content_unit, feature_content_unit,
                                                      feature_function_unit, feature_content_unit,
                                                      feature_content_unit, feature_content_unit},
          "closed-class English token is distinguished from content units");

    const std::vector<ExpectedSubunit> expected{
        {"Un", feature_derivational_affix},   {"believ", feature_lexical_core},
        {"able", feature_derivational_affix}, {"read", feature_lexical_core},
        {"er", feature_grammatical_affix},    {"s", feature_grammatical_affix},
        {"were", feature_lexical_core},       {"read", feature_lexical_core},
        {"ing", feature_grammatical_affix},   {"Quick", feature_lexical_core},
        {"ly", feature_derivational_affix},   {"re", feature_derivational_affix},
        {"play", feature_lexical_core},       {"ed", feature_grammatical_affix},
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
    std::vector<ExpectedSubunit> unicode_subunits;
    for (const auto& node : unicode_analysis.nodes) {
        if (node.kind == NodeKind::subunit) {
            unicode_subunits.push_back({slice(unicode, node), node.features.back().id});
        }
    }
    const std::vector<ExpectedSubunit> expected_unicode{
        {"Éclair", feature_lexical_core},
        {"can", feature_lexical_core},
        {"’t", feature_grammatical_affix},
    };
    check(unicode_subunits.size() == expected_unicode.size(),
          "curly-apostrophe contraction has explicit morphology");
    for (std::size_t index = 0; index < std::min(unicode_subunits.size(), expected_unicode.size());
         ++index) {
        check(unicode_subunits[index].text == expected_unicode[index].text,
              "Unicode morphology preserves byte-exact subunit text");
        check(unicode_subunits[index].feature == expected_unicode[index].feature,
              "Unicode morphology assigns the expected feature");
    }
    const auto unicode_sentence = unicode_analysis.nodes.front().children.front();
    const auto contraction_unit =
        unicode_analysis
            .nodes[unicode_analysis.nodes[unicode_sentence.value()].children.back().value()];
    check(has_feature(contraction_unit, feature_function_unit),
          "contracted auxiliary is classified as a function unit");

    const Text protected_functions("Under does");
    const auto protected_analysis = provider.analyze(protected_functions, "en");
    validate_analysis(protected_functions, protected_analysis);
    std::vector<std::string_view> protected_subunits;
    for (const auto& node : protected_analysis.nodes) {
        if (node.kind == NodeKind::subunit) {
            protected_subunits.push_back(slice(protected_functions, node));
        }
    }
    check(protected_subunits == std::vector<std::string_view>{"Under", "does"},
          "closed-class words are protected from coincidental affix matches");

    const Text layered("rereading unhappiness internationalization mischaracterizations");
    const auto layered_analysis = provider.analyze(layered, "en");
    validate_analysis(layered, layered_analysis);
    std::vector<std::string_view> layered_subunits;
    for (const auto& node : layered_analysis.nodes) {
        if (node.kind == NodeKind::subunit) {
            layered_subunits.push_back(slice(layered, node));
        }
    }
    check(layered_subunits == std::vector<std::string_view>{"re", "read", "ing", "un", "happi",
                                                            "ness", "inter", "nation", "al",
                                                            "ization", "mis", "character",
                                                            "ization", "s"},
          "layered morphology preserves ordered lexical and affix spans");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core English provider tests passed\n";
    return EXIT_SUCCESS;
}
