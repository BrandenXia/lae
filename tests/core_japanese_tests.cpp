#include "core/ir.hpp"
#include "core/text.hpp"
#include "providers/japanese/japanese_provider.hpp"

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

std::vector<std::string_view> children(const le::core::Text& text,
                                       const le::core::Analysis& analysis,
                                       const le::core::Node& parent) {
    std::vector<std::string_view> result;
    for (const auto child : parent.children) {
        result.push_back(slice(text, analysis.nodes[child.value()]));
    }
    return result;
}

const le::core::Node& sentence(const le::core::Analysis& analysis, std::size_t index) {
    return analysis.nodes[analysis.nodes.front().children.at(index).value()];
}

const le::core::Node& child(const le::core::Analysis& analysis, const le::core::Node& parent,
                            std::size_t index) {
    return analysis.nodes[parent.children.at(index).value()];
}

} // namespace

int main() {
    using namespace le::core;

    const JapaneseLanguageProvider provider;
    check(provider.supports("ja"), "provider supports ja");
    check(provider.supports("ja-JP"), "provider supports regional Japanese tag");
    check(provider.supports("JA"), "provider language matching is case-insensitive");
    check(!provider.supports("jpn"), "provider rejects unrelated language prefixes");
    check(!provider.supports("zh"), "provider rejects Chinese tags");

    const Text text("食べさせられました。日本語を研究しています！");
    const auto analysis = provider.analyze(text, "ja-JP");
    validate_analysis(text, analysis);
    check(analysis.nodes.front().children.size() == 2,
          "Japanese punctuation creates two sentence nodes");

    const auto& first_sentence = sentence(analysis, 0);
    check(children(text, analysis, first_sentence) ==
              std::vector<std::string_view>{"食べさせられました"},
          "mixed Han and hiragana form one morphological unit");
    const auto& inflected = child(analysis, first_sentence, 0);
    check(children(text, analysis, inflected) ==
              std::vector<std::string_view>{"食べ", "させられました"},
          "longest grammatical suffix preserves the lexical core");
    const auto& inflected_core = child(analysis, inflected, 0);
    const auto& inflected_suffix = child(analysis, inflected, 1);
    check(has_feature(inflected_core, feature_lexical_core),
          "stem subunit carries lexical-core feature");
    check(has_feature(inflected_core, feature_script_han) &&
              has_feature(inflected_core, feature_script_hiragana),
          "mixed-script lexical core preserves both script facts");
    check(has_feature(inflected_suffix, feature_grammatical_affix),
          "inflection carries grammatical-affix feature");
    check(has_feature(inflected_suffix, feature_script_hiragana),
          "inflection carries hiragana feature");

    const auto& second_sentence = sentence(analysis, 1);
    check(children(text, analysis, second_sentence) ==
              std::vector<std::string_view>{"日本語を", "研究しています"},
          "script transitions delimit Japanese units without whitespace");
    const auto& noun_particle = child(analysis, second_sentence, 0);
    check(children(text, analysis, noun_particle) == std::vector<std::string_view>{"日本語", "を"},
          "case particle is separated from its lexical core");
    check(has_feature(child(analysis, noun_particle, 1), feature_grammatical_affix),
          "particle carries grammatical feature");
    const auto& progressive = child(analysis, second_sentence, 1);
    check(children(text, analysis, progressive) ==
              std::vector<std::string_view>{"研究", "しています"},
          "auxiliary sequence is separated deterministically");
    check(has_feature(progressive, feature_content_unit),
          "unit containing a lexical core is content-bearing");
    check(has_feature(progressive, feature_segmentation_confidence),
          "recognized morphology has full segmentation confidence");

    const Text scripts("コンピューターを学ぶ。Swiftを使います。");
    const auto script_analysis = provider.analyze(scripts, "ja");
    validate_analysis(scripts, script_analysis);
    const auto& script_sentence = sentence(script_analysis, 0);
    check(children(scripts, script_analysis, script_sentence) ==
              std::vector<std::string_view>{"コンピューターを", "学ぶ"},
          "katakana and Han runs retain following okurigana or particles");
    const auto& katakana_unit = child(script_analysis, script_sentence, 0);
    check(has_feature(katakana_unit, feature_script_katakana),
          "katakana unit carries katakana script feature");
    check(children(scripts, script_analysis, katakana_unit) ==
              std::vector<std::string_view>{"コンピューター", "を"},
          "katakana loanword separates its particle");
    const auto& latin_unit = child(script_analysis, sentence(script_analysis, 1), 0);
    check(has_feature(latin_unit, feature_script_latin),
          "embedded Latin unit carries Latin script feature");

    const Text hiragana("これは。です。");
    const auto hiragana_analysis = provider.analyze(hiragana, "ja");
    validate_analysis(hiragana, hiragana_analysis);
    const auto& demonstrative = child(hiragana_analysis, sentence(hiragana_analysis, 0), 0);
    check(children(hiragana, hiragana_analysis, demonstrative) ==
              std::vector<std::string_view>{"これ", "は"},
          "hiragana-only unit can separate a final particle");
    const auto& copula = child(hiragana_analysis, sentence(hiragana_analysis, 1), 0);
    check(children(hiragana, hiragana_analysis, copula) == std::vector<std::string_view>{"です"},
          "standalone grammatical form remains one subunit");
    check(has_feature(child(hiragana_analysis, copula, 0), feature_grammatical_affix),
          "standalone copula is grammatical rather than lexical");
    check(!has_feature(copula, feature_content_unit),
          "standalone grammatical unit is not marked content-bearing");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core Japanese provider tests passed\n";
    return EXIT_SUCCESS;
}
