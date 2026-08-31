#include "providers/japanese/japanese_provider.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <utf8proc.h>

namespace le::core {
namespace {

enum class Script : std::uint8_t {
    other,
    han,
    hiragana,
    katakana,
    latin,
};

struct SubunitSpec {
    std::size_t begin;
    std::size_t end;
    FeatureId morphology;
};

struct UnitSpec {
    std::size_t begin;
    std::size_t end;
    std::vector<SubunitSpec> subunits;
    float segmentation_confidence;
};

struct SentenceSpec {
    TextSpan span;
    std::vector<UnitSpec> units;
};

struct SuffixRule {
    std::string_view text;
    FeatureId feature;
    bool standalone;
};

constexpr std::array suffixes{
    SuffixRule{"させられました", feature_grammatical_affix, true},
    SuffixRule{"ませんでした", feature_grammatical_affix, true},
    SuffixRule{"していました", feature_grammatical_affix, true},
    SuffixRule{"させられる", feature_grammatical_affix, true},
    SuffixRule{"られました", feature_grammatical_affix, true},
    SuffixRule{"しています", feature_grammatical_affix, true},
    SuffixRule{"させました", feature_grammatical_affix, true},
    SuffixRule{"しません", feature_grammatical_affix, true},
    SuffixRule{"られる", feature_grammatical_affix, true},
    SuffixRule{"しました", feature_grammatical_affix, true},
    SuffixRule{"でした", feature_grammatical_affix, true},
    SuffixRule{"ません", feature_grammatical_affix, true},
    SuffixRule{"ました", feature_grammatical_affix, true},
    SuffixRule{"やすい", feature_derivational_affix, false},
    SuffixRule{"にくい", feature_derivational_affix, false},
    SuffixRule{"から", feature_grammatical_affix, true},
    SuffixRule{"まで", feature_grammatical_affix, true},
    SuffixRule{"より", feature_grammatical_affix, true},
    SuffixRule{"ので", feature_grammatical_affix, true},
    SuffixRule{"のに", feature_grammatical_affix, true},
    SuffixRule{"には", feature_grammatical_affix, true},
    SuffixRule{"では", feature_grammatical_affix, true},
    SuffixRule{"ても", feature_grammatical_affix, true},
    SuffixRule{"だけ", feature_grammatical_affix, true},
    SuffixRule{"ほど", feature_grammatical_affix, true},
    SuffixRule{"ます", feature_grammatical_affix, true},
    SuffixRule{"です", feature_grammatical_affix, true},
    SuffixRule{"ない", feature_grammatical_affix, true},
    SuffixRule{"は", feature_grammatical_affix, true},
    SuffixRule{"が", feature_grammatical_affix, true},
    SuffixRule{"を", feature_grammatical_affix, true},
    SuffixRule{"に", feature_grammatical_affix, true},
    SuffixRule{"へ", feature_grammatical_affix, true},
    SuffixRule{"と", feature_grammatical_affix, true},
    SuffixRule{"で", feature_grammatical_affix, true},
    SuffixRule{"の", feature_grammatical_affix, true},
    SuffixRule{"も", feature_grammatical_affix, true},
    SuffixRule{"や", feature_grammatical_affix, true},
    SuffixRule{"ね", feature_grammatical_affix, true},
    SuffixRule{"よ", feature_grammatical_affix, true},
};

bool is_han(std::int32_t code_point) {
    return (code_point >= 0x3400 && code_point <= 0x4DBF) ||
           (code_point >= 0x4E00 && code_point <= 0x9FFF) ||
           (code_point >= 0xF900 && code_point <= 0xFAFF) ||
           (code_point >= 0x20000 && code_point <= 0x2EE5F) ||
           (code_point >= 0x30000 && code_point <= 0x323AF);
}

bool is_hiragana(std::int32_t code_point) {
    return (code_point >= 0x3040 && code_point <= 0x309F) || code_point == 0x1B001 ||
           (code_point >= 0x1B11F && code_point <= 0x1B122);
}

bool is_katakana(std::int32_t code_point) {
    return (code_point >= 0x30A0 && code_point <= 0x30FF) ||
           (code_point >= 0x31F0 && code_point <= 0x31FF) ||
           (code_point >= 0xFF66 && code_point <= 0xFF9D) || code_point == 0x1B000 ||
           (code_point >= 0x1B164 && code_point <= 0x1B167);
}

bool is_latin(std::int32_t code_point) {
    return (code_point >= 'A' && code_point <= 'Z') || (code_point >= 'a' && code_point <= 'z') ||
           (code_point >= 0x00C0 && code_point <= 0x024F) ||
           (code_point >= 0x1E00 && code_point <= 0x1EFF) ||
           (code_point >= 0xA720 && code_point <= 0xA7FF) ||
           (code_point >= 0xAB30 && code_point <= 0xAB6F) ||
           (code_point >= 0xFF21 && code_point <= 0xFF3A) ||
           (code_point >= 0xFF41 && code_point <= 0xFF5A);
}

bool is_word_like(std::int32_t code_point) {
    switch (utf8proc_category(code_point)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_ND:
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_NO:
        return true;
    default:
        return false;
    }
}

bool is_sentence_terminal(std::int32_t code_point) {
    return code_point == '.' || code_point == '!' || code_point == '?' || code_point == 0x3002 ||
           code_point == 0xFF01 || code_point == 0xFF1F;
}

Script script_of(std::int32_t code_point) {
    if (is_han(code_point)) {
        return Script::han;
    }
    if (is_hiragana(code_point)) {
        return Script::hiragana;
    }
    if (is_katakana(code_point)) {
        return Script::katakana;
    }
    if (is_latin(code_point)) {
        return Script::latin;
    }
    return Script::other;
}

FeatureId script_feature(Script script) {
    switch (script) {
    case Script::han:
        return feature_script_han;
    case Script::hiragana:
        return feature_script_hiragana;
    case Script::katakana:
        return feature_script_katakana;
    case Script::latin:
        return feature_script_latin;
    case Script::other:
        return 0;
    }
    return 0;
}

std::vector<Feature> script_features(const std::vector<Grapheme>& graphemes, std::size_t begin,
                                     std::size_t end) {
    std::vector<Feature> result;
    for (std::size_t index = begin; index < end; ++index) {
        const auto feature = script_feature(script_of(graphemes[index].first_code_point));
        if (feature != 0 && std::ranges::none_of(result, [feature](const Feature& item) {
                return item.id == feature;
            })) {
            result.push_back(Feature{feature, 1.0F});
        }
    }
    return result;
}

std::size_t grapheme_at_byte(const std::vector<Grapheme>& graphemes, std::size_t begin,
                             std::size_t end, std::uint64_t byte) {
    for (std::size_t index = begin; index < end; ++index) {
        if (graphemes[index].span.begin().value() == byte) {
            return index;
        }
    }
    return end;
}

UnitSpec make_unit(const Text& text, std::size_t begin, std::size_t end) {
    const auto& graphemes = text.graphemes();
    const auto byte_begin = static_cast<std::size_t>(graphemes[begin].span.begin().value());
    const auto byte_end = static_cast<std::size_t>(graphemes[end - 1].span.end().value());
    const auto unit_text = text.bytes().substr(byte_begin, byte_end - byte_begin);

    for (const auto& suffix : suffixes) {
        if (!unit_text.ends_with(suffix.text)) {
            continue;
        }
        const auto suffix_byte = byte_end - suffix.text.size();
        const auto suffix_begin = grapheme_at_byte(graphemes, begin, end, suffix_byte);
        if (suffix_begin == end) {
            continue;
        }
        if (suffix_begin == begin) {
            if (suffix.standalone && suffix.text.size() == unit_text.size()) {
                return UnitSpec{begin, end, {{begin, end, suffix.feature}}, 1.0F};
            }
            continue;
        }
        return UnitSpec{
            begin,
            end,
            {{begin, suffix_begin, feature_lexical_core}, {suffix_begin, end, suffix.feature}},
            1.0F};
    }
    return UnitSpec{begin, end, {{begin, end, feature_lexical_core}}, 0.5F};
}

std::vector<SentenceSpec> segment(const Text& text) {
    const auto& graphemes = text.graphemes();
    std::vector<SentenceSpec> sentences;
    std::vector<UnitSpec> units;
    std::size_t index = 0;

    auto finish_sentence = [&](ByteOffset end) {
        if (units.empty()) {
            return;
        }
        const auto begin = graphemes[units.front().begin].span.begin();
        sentences.push_back(SentenceSpec{TextSpan(begin, end), std::move(units)});
        units.clear();
    };

    while (index < graphemes.size()) {
        if (is_word_like(graphemes[index].first_code_point)) {
            const auto begin = index;
            const auto script = script_of(graphemes[index].first_code_point);
            while (index < graphemes.size() && is_word_like(graphemes[index].first_code_point) &&
                   script_of(graphemes[index].first_code_point) == script) {
                ++index;
            }
            if (script != Script::hiragana && index < graphemes.size() &&
                script_of(graphemes[index].first_code_point) == Script::hiragana) {
                while (index < graphemes.size() &&
                       script_of(graphemes[index].first_code_point) == Script::hiragana) {
                    ++index;
                }
            }
            units.push_back(make_unit(text, begin, index));
            continue;
        }
        if (is_sentence_terminal(graphemes[index].first_code_point)) {
            finish_sentence(graphemes[index].span.end());
        }
        ++index;
    }
    if (!units.empty()) {
        finish_sentence(graphemes[units.back().end - 1].span.end());
    }
    return sentences;
}

} // namespace

bool JapaneseLanguageProvider::supports(std::string_view language) const {
    return language.size() >= 2 && (language[0] == 'j' || language[0] == 'J') &&
           (language[1] == 'a' || language[1] == 'A') &&
           (language.size() == 2 || language[2] == '-');
}

Analysis JapaneseLanguageProvider::analyze(const Text& text, std::string_view language) const {
    Analysis result;
    result.nodes.push_back(
        Node{NodeId(0),
             TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())),
             NodeKind::document,
             {},
             {Feature{feature_grapheme_count, static_cast<float>(text.graphemes().size())}}});
    result.language_regions.push_back(LanguageRegion{
        TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())), std::string(language), 1.0F});

    const auto& graphemes = text.graphemes();
    for (const auto& sentence : segment(text)) {
        const auto sentence_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
        result.nodes.push_back(Node{sentence_id,
                                    sentence.span,
                                    NodeKind::sentence,
                                    {},
                                    {Feature{feature_boundary_strength, 1.0F}}});
        result.nodes.front().children.push_back(sentence_id);

        for (const auto& unit : sentence.units) {
            const auto unit_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
            auto features = script_features(graphemes, unit.begin, unit.end);
            features.push_back(
                Feature{feature_grapheme_count, static_cast<float>(unit.end - unit.begin)});
            features.push_back(
                Feature{feature_segmentation_confidence, unit.segmentation_confidence});
            if (std::ranges::any_of(unit.subunits, [](const SubunitSpec& subunit) {
                    return subunit.morphology == feature_lexical_core;
                })) {
                features.push_back(Feature{feature_content_unit, 1.0F});
            }
            result.nodes.push_back(Node{
                unit_id,
                TextSpan(graphemes[unit.begin].span.begin(), graphemes[unit.end - 1].span.end()),
                NodeKind::unit,
                {},
                std::move(features),
            });
            result.nodes[sentence_id.value()].children.push_back(unit_id);

            for (const auto& subunit : unit.subunits) {
                const auto subunit_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
                auto subunit_features = script_features(graphemes, subunit.begin, subunit.end);
                subunit_features.push_back(Feature{subunit.morphology, 1.0F});
                result.nodes.push_back(Node{
                    subunit_id,
                    TextSpan(graphemes[subunit.begin].span.begin(),
                             graphemes[subunit.end - 1].span.end()),
                    NodeKind::subunit,
                    {},
                    std::move(subunit_features),
                });
                result.nodes[unit_id.value()].children.push_back(subunit_id);
            }
        }
    }
    return result;
}

} // namespace le::core
