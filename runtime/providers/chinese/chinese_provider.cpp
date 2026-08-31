#include "providers/chinese/chinese_provider.hpp"

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

struct UnitSpec {
    TextSpan span;
    std::vector<TextSpan> characters;
    FeatureId script;
    FeatureId semantic;
    float segmentation_confidence;
};

struct SentenceSpec {
    TextSpan span;
    std::vector<UnitSpec> units;
};

struct SegmentationChoice {
    std::size_t known_graphemes;
    std::size_t unit_count;
    std::size_t next;
    std::size_t length;
    bool dictionary_match;
    FeatureId semantic;
};

struct LexiconEntry {
    std::string_view text;
    FeatureId semantic;
};

constexpr std::array lexicon{
    LexiconEntry{"中华", feature_content_unit},     LexiconEntry{"中華", feature_content_unit},
    LexiconEntry{"人民", feature_content_unit},     LexiconEntry{"共和国", feature_content_unit},
    LexiconEntry{"共和國", feature_content_unit},   LexiconEntry{"中文", feature_content_unit},
    LexiconEntry{"语言", feature_content_unit},     LexiconEntry{"語言", feature_content_unit},
    LexiconEntry{"框架", feature_content_unit},     LexiconEntry{"分词", feature_content_unit},
    LexiconEntry{"分詞", feature_content_unit},     LexiconEntry{"没有", feature_function_unit},
    LexiconEntry{"沒有", feature_function_unit},    LexiconEntry{"空格", feature_content_unit},
    LexiconEntry{"边界", feature_content_unit},     LexiconEntry{"邊界", feature_content_unit},
    LexiconEntry{"研究", feature_content_unit},     LexiconEntry{"研究生", feature_content_unit},
    LexiconEntry{"生命", feature_content_unit},     LexiconEntry{"起源", feature_content_unit},
    LexiconEntry{"南京", feature_content_unit},     LexiconEntry{"南京市", feature_content_unit},
    LexiconEntry{"市长", feature_content_unit},     LexiconEntry{"市長", feature_content_unit},
    LexiconEntry{"长江", feature_content_unit},     LexiconEntry{"長江", feature_content_unit},
    LexiconEntry{"大桥", feature_content_unit},     LexiconEntry{"大橋", feature_content_unit},
    LexiconEntry{"长江大桥", feature_content_unit}, LexiconEntry{"長江大橋", feature_content_unit},
    LexiconEntry{"重要", feature_content_unit},     LexiconEntry{"因为", feature_function_unit},
    LexiconEntry{"因為", feature_function_unit},    LexiconEntry{"所以", feature_function_unit},
    LexiconEntry{"但是", feature_function_unit},    LexiconEntry{"而且", feature_function_unit},
    LexiconEntry{"如果", feature_function_unit},    LexiconEntry{"虽然", feature_function_unit},
    LexiconEntry{"雖然", feature_function_unit},    LexiconEntry{"我", feature_function_unit},
    LexiconEntry{"你", feature_function_unit},      LexiconEntry{"他", feature_function_unit},
    LexiconEntry{"她", feature_function_unit},      LexiconEntry{"它", feature_function_unit},
    LexiconEntry{"们", feature_function_unit},      LexiconEntry{"們", feature_function_unit},
    LexiconEntry{"的", feature_function_unit},      LexiconEntry{"了", feature_function_unit},
    LexiconEntry{"在", feature_function_unit},      LexiconEntry{"是", feature_function_unit},
    LexiconEntry{"和", feature_function_unit},      LexiconEntry{"与", feature_function_unit},
    LexiconEntry{"與", feature_function_unit},      LexiconEntry{"及", feature_function_unit},
    LexiconEntry{"而", feature_function_unit},      LexiconEntry{"被", feature_function_unit},
    LexiconEntry{"把", feature_function_unit},      LexiconEntry{"对", feature_function_unit},
    LexiconEntry{"對", feature_function_unit},      LexiconEntry{"于", feature_function_unit},
    LexiconEntry{"於", feature_function_unit},      LexiconEntry{"着", feature_function_unit},
    LexiconEntry{"著", feature_function_unit},      LexiconEntry{"过", feature_function_unit},
    LexiconEntry{"過", feature_function_unit},      LexiconEntry{"吗", feature_function_unit},
    LexiconEntry{"嗎", feature_function_unit},      LexiconEntry{"呢", feature_function_unit},
    LexiconEntry{"吧", feature_function_unit},      LexiconEntry{"也", feature_function_unit},
    LexiconEntry{"都", feature_function_unit},      LexiconEntry{"很", feature_function_unit},
};

bool is_han(std::int32_t code_point) {
    return (code_point >= 0x3400 && code_point <= 0x4DBF) ||
           (code_point >= 0x4E00 && code_point <= 0x9FFF) ||
           (code_point >= 0xF900 && code_point <= 0xFAFF) ||
           (code_point >= 0x20000 && code_point <= 0x2EE5F) ||
           (code_point >= 0x30000 && code_point <= 0x323AF);
}

bool is_latin(std::int32_t code_point) {
    return (code_point >= 'A' && code_point <= 'Z') || (code_point >= 'a' && code_point <= 'z') ||
           (code_point >= 0x00C0 && code_point <= 0x024F) ||
           (code_point >= 0x1E00 && code_point <= 0x1EFF) ||
           (code_point >= 0xA720 && code_point <= 0xA7FF) ||
           (code_point >= 0xAB30 && code_point <= 0xAB6F) ||
           (code_point >= 0xFF21 && code_point <= 0xFF3A) ||
           (code_point >= 0xFF41 && code_point <= 0xFF5A) ||
           (code_point >= 0x10780 && code_point <= 0x107BF) ||
           (code_point >= 0x1DF00 && code_point <= 0x1DFFF);
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

std::size_t match_length(const Text& text, const std::vector<Grapheme>& graphemes,
                         std::size_t run_begin, std::size_t run_end, std::size_t position,
                         std::string_view word) {
    const auto absolute = run_begin + position;
    const auto byte_begin = static_cast<std::size_t>(graphemes[absolute].span.begin().value());
    if (!text.bytes().substr(byte_begin).starts_with(word)) {
        return 0;
    }
    const auto byte_end = byte_begin + word.size();
    for (std::size_t index = absolute; index < run_end; ++index) {
        if (graphemes[index].span.end().value() == byte_end) {
            return index - absolute + 1;
        }
        if (graphemes[index].span.end().value() > byte_end) {
            break;
        }
    }
    return 0;
}

bool better(const SegmentationChoice& candidate, const SegmentationChoice& current) {
    if (candidate.known_graphemes != current.known_graphemes) {
        return candidate.known_graphemes > current.known_graphemes;
    }
    if (candidate.unit_count != current.unit_count) {
        return candidate.unit_count < current.unit_count;
    }
    return candidate.length > current.length;
}

std::vector<UnitSpec> segment_han(const Text& text, std::size_t run_begin, std::size_t run_end) {
    const auto& graphemes = text.graphemes();
    const auto size = run_end - run_begin;
    std::vector<SegmentationChoice> choices(size + 1);
    choices[size] = SegmentationChoice{0, 0, size, 0, false, 0};

    for (std::size_t position = size; position-- > 0;) {
        const auto& fallback_tail = choices[position + 1];
        choices[position] = SegmentationChoice{
            fallback_tail.known_graphemes, fallback_tail.unit_count + 1, position + 1, 1, false, 0};
        for (const auto& entry : lexicon) {
            const auto length =
                match_length(text, graphemes, run_begin, run_end, position, entry.text);
            if (length == 0) {
                continue;
            }
            const auto& tail = choices[position + length];
            const SegmentationChoice candidate{tail.known_graphemes + length,
                                               tail.unit_count + 1,
                                               position + length,
                                               length,
                                               true,
                                               entry.semantic};
            if (better(candidate, choices[position])) {
                choices[position] = candidate;
            }
        }
    }

    std::vector<UnitSpec> units;
    for (std::size_t position = 0; position < size;) {
        const auto choice = choices[position];
        std::vector<TextSpan> characters;
        characters.reserve(choice.length);
        for (std::size_t index = position; index < choice.next; ++index) {
            characters.push_back(graphemes[run_begin + index].span);
        }
        units.push_back(UnitSpec{
            TextSpan(characters.front().begin(), characters.back().end()),
            std::move(characters),
            feature_script_han,
            choice.semantic,
            choice.dictionary_match ? 1.0F : 0.25F,
        });
        position = choice.next;
    }
    return units;
}

UnitSpec make_non_han_unit(const std::vector<Grapheme>& graphemes, std::size_t begin,
                           std::size_t end) {
    std::vector<TextSpan> characters;
    characters.reserve(end - begin);
    bool latin = true;
    for (std::size_t index = begin; index < end; ++index) {
        characters.push_back(graphemes[index].span);
        latin = latin && is_latin(graphemes[index].first_code_point);
    }
    return UnitSpec{
        TextSpan(characters.front().begin(), characters.back().end()),
        std::move(characters),
        latin ? feature_script_latin : FeatureId(0),
        feature_content_unit,
        0.5F,
    };
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
        sentences.push_back(
            SentenceSpec{TextSpan(units.front().span.begin(), end), std::move(units)});
        units.clear();
    };

    while (index < graphemes.size()) {
        if (is_han(graphemes[index].first_code_point)) {
            const auto begin = index;
            while (index < graphemes.size() && is_han(graphemes[index].first_code_point)) {
                ++index;
            }
            auto segmented = segment_han(text, begin, index);
            for (auto& unit : segmented) {
                units.push_back(std::move(unit));
            }
            continue;
        }
        if (is_word_like(graphemes[index].first_code_point)) {
            const auto begin = index;
            while (index < graphemes.size() && !is_han(graphemes[index].first_code_point) &&
                   is_word_like(graphemes[index].first_code_point)) {
                ++index;
            }
            units.push_back(make_non_han_unit(graphemes, begin, index));
            continue;
        }
        if (is_sentence_terminal(graphemes[index].first_code_point)) {
            finish_sentence(graphemes[index].span.end());
        }
        ++index;
    }
    if (!units.empty()) {
        finish_sentence(units.back().span.end());
    }
    return sentences;
}

} // namespace

bool ChineseLanguageProvider::supports(std::string_view language) const {
    return language.size() >= 2 && (language[0] == 'z' || language[0] == 'Z') &&
           (language[1] == 'h' || language[1] == 'H') &&
           (language.size() == 2 || language[2] == '-');
}

Analysis ChineseLanguageProvider::analyze(const Text& text, std::string_view language) const {
    Analysis result;
    result.nodes.push_back(
        Node{NodeId(0),
             TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())),
             NodeKind::document,
             {},
             {Feature{feature_grapheme_count, static_cast<float>(text.graphemes().size())}}});
    result.language_regions.push_back(LanguageRegion{
        TextSpan(ByteOffset(0), ByteOffset(text.bytes().size())), std::string(language), 1.0F});

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
            std::vector<Feature> features{
                Feature{feature_grapheme_count, static_cast<float>(unit.characters.size())},
                Feature{feature_segmentation_confidence, unit.segmentation_confidence},
                Feature{feature_lexical_core, 1.0F},
            };
            if (unit.script != 0) {
                features.push_back(Feature{unit.script, 1.0F});
            }
            if (unit.semantic != 0) {
                features.push_back(Feature{unit.semantic, 1.0F});
            }
            result.nodes.push_back(
                Node{unit_id, unit.span, NodeKind::unit, {}, std::move(features)});
            result.nodes[sentence_id.value()].children.push_back(unit_id);

            for (const auto character : unit.characters) {
                const auto character_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
                std::vector<Feature> character_features;
                if (unit.script != 0) {
                    character_features.push_back(Feature{unit.script, 1.0F});
                }
                result.nodes.push_back(Node{
                    character_id, character, NodeKind::subunit, {}, std::move(character_features)});
                result.nodes[unit_id.value()].children.push_back(character_id);
            }
        }
    }
    return result;
}

} // namespace le::core
