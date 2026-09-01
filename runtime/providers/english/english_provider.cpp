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
    AffixRule{"ization", feature_derivational_affix},
    AffixRule{"isation", feature_derivational_affix},
    AffixRule{"ation", feature_derivational_affix},
    AffixRule{"ition", feature_derivational_affix},
    AffixRule{"ology", feature_derivational_affix},
    AffixRule{"ment", feature_derivational_affix},
    AffixRule{"ness", feature_derivational_affix},
    AffixRule{"less", feature_derivational_affix},
    AffixRule{"able", feature_derivational_affix},
    AffixRule{"ible", feature_derivational_affix},
    AffixRule{"ful", feature_derivational_affix},
    AffixRule{"ous", feature_derivational_affix},
    AffixRule{"ive", feature_derivational_affix},
    AffixRule{"ism", feature_derivational_affix},
    AffixRule{"ist", feature_derivational_affix},
    AffixRule{"ize", feature_derivational_affix},
    AffixRule{"ise", feature_derivational_affix},
    AffixRule{"est", feature_grammatical_affix},
    AffixRule{"ing", feature_grammatical_affix},
    AffixRule{"ed", feature_grammatical_affix},
    AffixRule{"er", feature_grammatical_affix},
    AffixRule{"ly", feature_derivational_affix},
    AffixRule{"al", feature_derivational_affix},
    AffixRule{"s", feature_grammatical_affix},
};

constexpr std::array morphology_exceptions{
    std::string_view("address"),   std::string_view("analysis"), std::string_view("business"),
    std::string_view("character"), std::string_view("computer"), std::string_view("document"),
    std::string_view("nation"),    std::string_view("number"),   std::string_view("other"),
    std::string_view("paper"),     std::string_view("power"),    std::string_view("process"),
    std::string_view("progress"),  std::string_view("proper"),   std::string_view("signal"),
    std::string_view("success"),   std::string_view("summer"),   std::string_view("water"),
    std::string_view("winter"),    std::string_view("witness"),
};

bool is_morphology_exception(std::string_view value) {
    if (std::ranges::find(morphology_exceptions, value) != morphology_exceptions.end()) {
        return true;
    }
    return std::ranges::any_of(prefixes, [&](std::string_view prefix) {
        return value.starts_with(prefix) &&
               std::ranges::find(morphology_exceptions, value.substr(prefix.size())) !=
                   morphology_exceptions.end();
    });
}

constexpr std::array contractions{
    std::string_view("n't"), std::string_view("n’t"), std::string_view("'re"),
    std::string_view("’re"), std::string_view("'ve"), std::string_view("’ve"),
    std::string_view("'ll"), std::string_view("’ll"), std::string_view("'s"),
    std::string_view("’s"),  std::string_view("'d"),  std::string_view("’d"),
    std::string_view("'m"),  std::string_view("’m"),
};

constexpr std::array normalized_contractions{
    std::string_view("n't"), std::string_view("'re"), std::string_view("'ve"),
    std::string_view("'ll"), std::string_view("'s"),  std::string_view("'d"),
    std::string_view("'m"),
};

constexpr std::array function_words{
    std::string_view("a"),        std::string_view("about"),  std::string_view("after"),
    std::string_view("although"), std::string_view("am"),     std::string_view("an"),
    std::string_view("and"),      std::string_view("are"),    std::string_view("as"),
    std::string_view("at"),       std::string_view("be"),     std::string_view("because"),
    std::string_view("been"),     std::string_view("before"), std::string_view("being"),
    std::string_view("between"),  std::string_view("but"),    std::string_view("by"),
    std::string_view("can"),      std::string_view("could"),  std::string_view("did"),
    std::string_view("do"),       std::string_view("does"),   std::string_view("during"),
    std::string_view("for"),      std::string_view("from"),   std::string_view("had"),
    std::string_view("has"),      std::string_view("have"),   std::string_view("he"),
    std::string_view("her"),      std::string_view("him"),    std::string_view("his"),
    std::string_view("i"),        std::string_view("if"),     std::string_view("in"),
    std::string_view("into"),     std::string_view("is"),     std::string_view("it"),
    std::string_view("its"),      std::string_view("may"),    std::string_view("me"),
    std::string_view("might"),    std::string_view("must"),   std::string_view("no"),
    std::string_view("nor"),      std::string_view("not"),    std::string_view("of"),
    std::string_view("on"),       std::string_view("onto"),   std::string_view("or"),
    std::string_view("our"),      std::string_view("over"),   std::string_view("per"),
    std::string_view("shall"),    std::string_view("she"),    std::string_view("should"),
    std::string_view("so"),       std::string_view("than"),   std::string_view("that"),
    std::string_view("the"),      std::string_view("their"),  std::string_view("them"),
    std::string_view("then"),     std::string_view("these"),  std::string_view("they"),
    std::string_view("this"),     std::string_view("those"),  std::string_view("though"),
    std::string_view("through"),  std::string_view("to"),     std::string_view("under"),
    std::string_view("up"),       std::string_view("us"),     std::string_view("via"),
    std::string_view("was"),      std::string_view("we"),     std::string_view("were"),
    std::string_view("what"),     std::string_view("when"),   std::string_view("where"),
    std::string_view("which"),    std::string_view("while"),  std::string_view("who"),
    std::string_view("whom"),     std::string_view("whose"),  std::string_view("will"),
    std::string_view("with"),     std::string_view("within"), std::string_view("without"),
    std::string_view("would"),    std::string_view("you"),    std::string_view("your"),
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
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return character < 0x80 ? static_cast<char>(std::tolower(character))
                                : static_cast<char>(character);
    });
    return result;
}

std::string function_key(std::string_view token) {
    std::string result;
    result.reserve(token.size());
    for (std::size_t index = 0; index < token.size();) {
        const auto byte = static_cast<unsigned char>(token[index]);
        if (index + 3 <= token.size() && token.substr(index, 3) == "’") {
            result.push_back('\'');
            index += 3;
            continue;
        }
        if (byte >= 0x80) {
            return {};
        }
        result.push_back(static_cast<char>(std::tolower(byte)));
        ++index;
    }
    return result;
}

bool is_function_unit(std::string_view token) {
    const auto key = function_key(token);
    if (key.empty()) {
        return false;
    }
    if (std::ranges::find(function_words, key) != function_words.end()) {
        return true;
    }
    for (const auto contraction : normalized_contractions) {
        if (!key.ends_with(contraction) || key.size() == contraction.size()) {
            continue;
        }
        const auto base = key.substr(0, key.size() - contraction.size());
        if (std::ranges::find(function_words, base) != function_words.end()) {
            return true;
        }
        if (contraction == "n't" && (key == "can't" || key == "won't" || key == "shan't")) {
            return true;
        }
    }
    return false;
}

std::vector<SubunitSpec> analyze_morphology(std::string_view token) {
    const auto lower = ascii_lower(token);
    if (is_function_unit(token) && lower.find('\'') == std::string::npos &&
        lower.find("’") == std::string::npos) {
        return {{0, token.size(), feature_lexical_core}};
    }
    if (is_morphology_exception(lower)) {
        return {{0, token.size(), feature_lexical_core}};
    }

    std::size_t core_begin = 0;
    std::size_t core_end = lower.size();
    std::vector<SubunitSpec> leading;
    std::vector<SubunitSpec> trailing;

    const auto reduced_negative = lower == "can't" || lower == "can’t" || lower == "won't" ||
                                  lower == "won’t" || lower == "shan't" || lower == "shan’t";
    if (reduced_negative) {
        const auto suffix = lower.ends_with("’t") ? std::string_view("’t") : std::string_view("'t");
        trailing.push_back({core_end - suffix.size(), core_end, feature_grammatical_affix});
        core_end -= suffix.size();
    } else {
        for (const auto contraction : contractions) {
            if (ends_with(lower, contraction, core_end) &&
                core_end - contraction.size() > core_begin) {
                trailing.push_back(
                    {core_end - contraction.size(), core_end, feature_grammatical_affix});
                core_end -= contraction.size();
                break;
            }
        }
    }

    for (std::size_t layer = 0; layer < 3; ++layer) {
        if (is_morphology_exception(
                std::string_view(lower).substr(core_begin, core_end - core_begin))) {
            break;
        }
        bool matched = false;
        for (const auto& suffix : suffixes) {
            if (suffix.text == "s" && core_end >= 2 &&
                (lower[core_end - 2] == 's' || lower[core_end - 2] == 'u' ||
                 lower[core_end - 2] == 'i')) {
                continue;
            }
            if (suffix.text == "er" && core_end - core_begin < 6) {
                continue;
            }
            if (ends_with(lower, suffix.text, core_end) &&
                core_end - suffix.text.size() >= core_begin + 3) {
                trailing.push_back({core_end - suffix.text.size(), core_end, suffix.feature});
                core_end -= suffix.text.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            break;
        }
    }

    for (std::size_t layer = 0; layer < 3; ++layer) {
        bool matched = false;
        for (const auto prefix : prefixes) {
            const auto ambiguous_read =
                prefix == "re" &&
                lower.substr(core_begin, core_end - core_begin).starts_with("read");
            if (!ambiguous_read && starts_with(lower, prefix, core_begin) &&
                core_end - core_begin - prefix.size() >= 3) {
                leading.push_back(
                    {core_begin, core_begin + prefix.size(), feature_derivational_affix});
                core_begin += prefix.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            break;
        }
    }

    std::vector<SubunitSpec> result;
    result.reserve(leading.size() + trailing.size() + 1);
    result.insert(result.end(), leading.begin(), leading.end());
    result.push_back({core_begin, core_end, feature_lexical_core});
    for (auto item = trailing.rbegin(); item != trailing.rend(); ++item) {
        result.push_back(*item);
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

        for (std::size_t token_index = 0; token_index < sentence.tokens.size(); ++token_index) {
            const auto& token = sentence.tokens[token_index];
            const auto unit_id = NodeId(static_cast<std::uint32_t>(result.nodes.size()));
            const auto token_begin = static_cast<std::size_t>(token.span.begin().value());
            const auto token_size =
                static_cast<std::size_t>(token.span.end().value() - token.span.begin().value());
            const auto token_text = text.bytes().substr(token_begin, token_size);
            const auto sentence_progress = sentence.tokens.size() <= 1
                                               ? 0.0F
                                               : static_cast<float>(token_index) /
                                                     static_cast<float>(sentence.tokens.size() - 1);
            result.nodes.push_back(Node{
                unit_id,
                token.span,
                NodeKind::unit,
                {},
                {Feature{feature_grapheme_count, static_cast<float>(token.grapheme_count)},
                 Feature{feature_segmentation_confidence, 1.0F},
                 Feature{feature_unit_position, static_cast<float>(token_index + 1)},
                 Feature{feature_sentence_progress, sentence_progress},
                 Feature{feature_sentence_unit_count, static_cast<float>(sentence.tokens.size())},
                 Feature{is_function_unit(token_text) ? feature_function_unit
                                                      : feature_content_unit,
                         1.0F}},
            });
            result.nodes[sentence_id.value()].children.push_back(unit_id);

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
