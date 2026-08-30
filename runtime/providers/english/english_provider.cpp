#include "providers/english/english_provider.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <utf8proc.h>

namespace le::core {
namespace {

struct TokenSpec {
    TextSpan span;
    std::uint32_t grapheme_count;
};

struct SentenceSpec {
    TextSpan span;
    std::vector<TokenSpec> tokens;
};

struct SubunitSpec {
    std::size_t begin;
    std::size_t end;
    FeatureId feature;
};

struct AffixRule {
    std::string_view text;
    FeatureId feature;
};

constexpr std::array prefixes{
    std::string_view("under"), std::string_view("inter"), std::string_view("super"),
    std::string_view("trans"), std::string_view("anti"),  std::string_view("auto"),
    std::string_view("over"),  std::string_view("pre"),   std::string_view("sub"),
    std::string_view("dis"),   std::string_view("mis"),   std::string_view("non"),
    std::string_view("un"),    std::string_view("re"),
};

constexpr std::array suffixes{
    AffixRule{"ability", feature_derivational_affix},
    AffixRule{"ibility", feature_derivational_affix},
    AffixRule{"ation", feature_derivational_affix},
    AffixRule{"ition", feature_derivational_affix},
    AffixRule{"ment", feature_derivational_affix},
    AffixRule{"ness", feature_derivational_affix},
    AffixRule{"less", feature_derivational_affix},
    AffixRule{"able", feature_derivational_affix},
    AffixRule{"ible", feature_derivational_affix},
    AffixRule{"ful", feature_derivational_affix},
    AffixRule{"ous", feature_derivational_affix},
    AffixRule{"ive", feature_derivational_affix},
    AffixRule{"est", feature_grammatical_affix},
    AffixRule{"ing", feature_grammatical_affix},
    AffixRule{"ed", feature_grammatical_affix},
    AffixRule{"er", feature_grammatical_affix},
    AffixRule{"ly", feature_derivational_affix},
    AffixRule{"al", feature_derivational_affix},
    AffixRule{"s", feature_grammatical_affix},
};

constexpr std::array contractions{
    std::string_view("n't"), std::string_view("'re"), std::string_view("'ve"),
    std::string_view("'ll"), std::string_view("'s"),  std::string_view("'d"),
    std::string_view("'m"),
};

bool is_letter(std::int32_t code_point) {
    switch (utf8proc_category(code_point)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
        return true;
    default:
        return false;
    }
}

bool is_apostrophe(std::int32_t code_point) { return code_point == 0x27 || code_point == 0x2019; }

bool is_sentence_terminal(std::int32_t code_point) {
    return code_point == '.' || code_point == '!' || code_point == '?';
}

bool starts_with(std::string_view text, std::string_view prefix, std::size_t offset) {
    return offset <= text.size() && text.substr(offset).starts_with(prefix);
}

bool ends_with(std::string_view text, std::string_view suffix, std::size_t end) {
    return suffix.size() <= end && text.substr(0, end).ends_with(suffix);
}

std::string ascii_lower(std::string_view text) {
    if (std::ranges::any_of(
            text, [](char character) { return static_cast<unsigned char>(character) >= 0x80; })) {
        return {};
    }
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::vector<SubunitSpec> analyze_morphology(std::string_view token) {
    const auto lower = ascii_lower(token);
    if (lower.empty()) {
        return {{0, token.size(), feature_lexical_core}};
    }

    std::size_t core_begin = 0;
    std::size_t core_end = lower.size();
    std::vector<SubunitSpec> result;

    FeatureId suffix_feature = 0;
    for (const auto contraction : contractions) {
        if (ends_with(lower, contraction, core_end) && core_end - contraction.size() > core_begin) {
            core_end -= contraction.size();
            suffix_feature = feature_grammatical_affix;
            break;
        }
    }
    if (suffix_feature == 0) {
        for (const auto& suffix : suffixes) {
            if (ends_with(lower, suffix.text, core_end) &&
                core_end - suffix.text.size() >= core_begin + 3) {
                core_end -= suffix.text.size();
                suffix_feature = suffix.feature;
                break;
            }
        }
    }

    for (const auto prefix : prefixes) {
        const auto ambiguous_read = prefix == "re" && lower.starts_with("read");
        if (!ambiguous_read && starts_with(lower, prefix, 0) && core_end - prefix.size() >= 3) {
            core_begin = prefix.size();
            break;
        }
    }

    if (core_begin != 0) {
        result.push_back({0, core_begin, feature_derivational_affix});
    }
    result.push_back({core_begin, core_end, feature_lexical_core});
    if (core_end < lower.size()) {
        result.push_back({core_end, lower.size(), suffix_feature});
    }
    return result;
}

std::vector<SentenceSpec> segment(const Text& text) {
    const auto& graphemes = text.graphemes();
    std::vector<SentenceSpec> sentences;
    std::vector<TokenSpec> tokens;
    std::size_t index = 0;

    auto finish_sentence = [&](ByteOffset end) {
        if (tokens.empty()) {
            return;
        }
        sentences.push_back(
            SentenceSpec{TextSpan(tokens.front().span.begin(), end), std::move(tokens)});
        tokens.clear();
    };

    while (index < graphemes.size()) {
        if (is_letter(graphemes[index].first_code_point)) {
            const auto begin = index;
            ++index;
            while (index < graphemes.size()) {
                if (is_letter(graphemes[index].first_code_point)) {
                    ++index;
                    continue;
                }
                if (is_apostrophe(graphemes[index].first_code_point) &&
                    index + 1 < graphemes.size() &&
                    is_letter(graphemes[index + 1].first_code_point)) {
                    index += 2;
                    continue;
                }
                break;
            }
            tokens.push_back(
                TokenSpec{TextSpan(graphemes[begin].span.begin(), graphemes[index - 1].span.end()),
                          static_cast<std::uint32_t>(index - begin)});
            continue;
        }
        if (is_sentence_terminal(graphemes[index].first_code_point)) {
            finish_sentence(graphemes[index].span.end());
        }
        ++index;
    }
    if (!tokens.empty()) {
        finish_sentence(tokens.back().span.end());
    }
    return sentences;
}

} // namespace

bool EnglishLanguageProvider::supports(std::string_view language) const {
    return language.size() >= 2 && (language[0] == 'e' || language[0] == 'E') &&
           (language[1] == 'n' || language[1] == 'N') &&
           (language.size() == 2 || language[2] == '-');
}

Analysis EnglishLanguageProvider::analyze(const Text& text, std::string_view language) const {
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

        for (const auto& token : sentence.tokens) {
            const auto unit_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
            result.nodes.push_back(
                Node{unit_id,
                     token.span,
                     NodeKind::unit,
                     {},
                     {Feature{feature_grapheme_count, static_cast<float>(token.grapheme_count)}}});
            result.nodes[sentence_id.value()].children.push_back(unit_id);

            const auto token_begin = static_cast<std::size_t>(token.span.begin().value());
            const auto token_size =
                static_cast<std::size_t>(token.span.end().value() - token.span.begin().value());
            const auto token_text = text.bytes().substr(token_begin, token_size);
            for (const auto& subunit : analyze_morphology(token_text)) {
                const auto subunit_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
                result.nodes.push_back(Node{subunit_id,
                                            TextSpan(ByteOffset(token_begin + subunit.begin),
                                                     ByteOffset(token_begin + subunit.end)),
                                            NodeKind::subunit,
                                            {},
                                            {Feature{subunit.feature, 1.0F}}});
                result.nodes[unit_id.value()].children.push_back(subunit_id);
            }
        }
    }
    return result;
}

} // namespace le::core
