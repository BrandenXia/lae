#include "core/ir.hpp"
#include "core/text.hpp"
#include "providers/chinese/chinese_provider.hpp"

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

bool has_feature(const le::core::Node& node, le::core::FeatureId id, float value) {
    return std::ranges::any_of(node.features, [=](const le::core::Feature& feature) {
        return feature.id == id && feature.value == value;
    });
}

std::vector<std::string_view> sentence_units(const le::core::Text& text,
                                             const le::core::Analysis& analysis,
                                             std::size_t sentence_index) {
    std::vector<std::string_view> result;
    const auto sentence_id = analysis.nodes.front().children.at(sentence_index);
    for (const auto unit_id : analysis.nodes[sentence_id.value()].children) {
        result.push_back(slice(text, analysis.nodes[unit_id.value()]));
    }
    return result;
}

} // namespace

int main() {
    using namespace le::core;

    const ChineseLanguageProvider provider;
    check(provider.supports("zh"), "provider supports zh");
    check(provider.supports("zh-Hans"), "provider supports simplified Chinese tag");
    check(provider.supports("zh-Hant"), "provider supports traditional Chinese tag");
    check(provider.supports("ZH"), "provider language matching is case-insensitive");
    check(!provider.supports("zho"), "provider rejects unrelated language prefixes");
    check(!provider.supports("ja"), "provider rejects Japanese tags");

    const Text text("中华人民共和国。研究生命起源！");
    const auto analysis = provider.analyze(text, "zh-Hans");
    validate_analysis(text, analysis);
    check(analysis.nodes.front().children.size() == 2,
          "Chinese punctuation creates two sentence nodes");

    const std::vector<std::string_view> first_expected{"中华", "人民", "共和国"};
    const std::vector<std::string_view> second_expected{"研究", "生命", "起源"};
    check(sentence_units(text, analysis, 0) == first_expected,
          "unspaced Chinese text segments into lexical units");
    check(sentence_units(text, analysis, 1) == second_expected,
          "ambiguous prefix resolves to fully covered segmentation");

    const Text traditional_text("中華人民共和國");
    const auto traditional = provider.analyze(traditional_text, "zh-Hant");
    validate_analysis(traditional_text, traditional);
    check(sentence_units(traditional_text, traditional, 0) ==
              std::vector<std::string_view>{"中華", "人民", "共和國"},
          "traditional Chinese uses the same unit structure");

    for (const auto sentence_id : analysis.nodes.front().children) {
        for (const auto unit_id : analysis.nodes[sentence_id.value()].children) {
            const auto& unit = analysis.nodes[unit_id.value()];
            check(has_feature(unit, feature_lexical_core, 1.0F),
                  "segmented unit is a lexical core");
            check(has_feature(unit, feature_script_han, 1.0F),
                  "segmented unit carries Han script feature");
            check(has_feature(unit, feature_segmentation_confidence, 1.0F),
                  "dictionary unit has full segmentation confidence");
            check(has_feature(unit, feature_content_unit, 1.0F),
                  "dictionary lexical unit is content-bearing");
            check(!unit.children.empty(), "word unit owns character subunits");
            for (const auto character_id : unit.children) {
                const auto& character = analysis.nodes[character_id.value()];
                check(character.kind == NodeKind::subunit,
                      "Chinese character is represented as a subunit");
                check(has_feature(character, feature_script_han, 1.0F),
                      "character subunit carries Han script feature");
            }
        }
    }

    const Text fallback_text("甲乙");
    const auto fallback = provider.analyze(fallback_text, "zh");
    validate_analysis(fallback_text, fallback);
    check(sentence_units(fallback_text, fallback, 0) == std::vector<std::string_view>{"甲", "乙"},
          "unknown Han sequence falls back to character units");
    for (const auto unit_id :
         fallback.nodes[fallback.nodes.front().children.front().value()].children) {
        check(has_feature(fallback.nodes[unit_id.value()], feature_segmentation_confidence, 0.25F),
              "fallback character has low segmentation confidence");
        check(!has_feature(fallback.nodes[unit_id.value()], feature_content_unit, 1.0F) &&
                  !has_feature(fallback.nodes[unit_id.value()], feature_function_unit, 1.0F),
              "unknown character does not receive a guessed semantic class");
    }

    const Text scripts("𠀀中文AI");
    const auto script_analysis = provider.analyze(scripts, "zh-Hant");
    validate_analysis(scripts, script_analysis);
    check(sentence_units(scripts, script_analysis, 0) ==
              std::vector<std::string_view>{"𠀀", "中文", "AI"},
          "extension-B Han and adjacent Latin run retain distinct units");
    const auto script_sentence = script_analysis.nodes.front().children.front();
    const auto& latin_unit =
        script_analysis
            .nodes[script_analysis.nodes[script_sentence.value()].children.back().value()];
    check(has_feature(latin_unit, feature_script_latin, 1.0F),
          "Latin run carries Latin script feature");
    check(has_feature(latin_unit, feature_content_unit, 1.0F),
          "embedded Latin run is content-bearing");

    const Text semantic_text("我在研究语言，因为它很重要。");
    const auto semantic = provider.analyze(semantic_text, "zh-Hans");
    validate_analysis(semantic_text, semantic);
    check(sentence_units(semantic_text, semantic, 0) ==
              std::vector<std::string_view>{"我", "在", "研究", "语言", "因为", "它", "很", "重要"},
          "Chinese segmentation recognizes function and content units together");
    const auto semantic_sentence = semantic.nodes.front().children.front();
    const auto& semantic_units = semantic.nodes[semantic_sentence.value()].children;
    const std::vector<FeatureId> expected_semantics{
        feature_function_unit, feature_function_unit, feature_content_unit,  feature_content_unit,
        feature_function_unit, feature_function_unit, feature_function_unit, feature_content_unit,
    };
    check(semantic_units.size() == expected_semantics.size(),
          "semantic Chinese sample has the expected unit count");
    for (std::size_t index = 0; index < std::min(semantic_units.size(), expected_semantics.size());
         ++index) {
        check(has_feature(semantic.nodes[semantic_units[index].value()], expected_semantics[index],
                          1.0F),
              "Chinese unit receives its provider-neutral semantic class");
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core Chinese provider tests passed\n";
    return EXIT_SUCCESS;
}
