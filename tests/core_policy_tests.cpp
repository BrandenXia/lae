#include "core/emphasis.hpp"
#include "core/reading.hpp"
#include "core/text.hpp"

#include <cmath>
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

bool near(float left, float right) { return std::fabs(left - right) < 0.0001F; }

} // namespace

int main() {
    using namespace le::core;

    const std::vector<ReadingSignal> signals{
        {TextSpan(ByteOffset(0), ByteOffset(1)), 1.0F, 0.0F, 0.0F},
        {TextSpan(ByteOffset(1), ByteOffset(2)), 0.75F, 0.0F, 0.0F},
        {TextSpan(ByteOffset(2), ByteOffset(3)), 0.5F, 0.0F, 0.0F},
        {TextSpan(ByteOffset(4), ByteOffset(5)), 0.25F, 0.0F, 0.0F},
    };

    const auto binary = generate_emphasis(
        signals, PresentationConfig{PresentationPolicy::binary, 0.5F, 0.0F, 0.8F});
    check(binary.size() == 1, "binary policy merges adjacent equal-strength signals");
    check(binary[0].span.begin().value() == 0 && binary[0].span.end().value() == 3,
          "binary policy selects signals at threshold");
    check(near(binary[0].strength, 0.8F), "binary policy uses maximum strength");

    const auto variable = generate_emphasis(
        signals, PresentationConfig{PresentationPolicy::variable_strength, 0.5F, 0.2F, 1.0F});
    check(variable.size() == 3, "variable policy retains distinct strengths");
    check(near(variable[0].strength, 1.0F), "variable policy maps salience one to maximum");
    check(near(variable[1].strength, 0.8F), "variable policy interpolates middle salience");
    check(near(variable[2].strength, 0.6F), "variable policy maps threshold salience");

    const Analysis analysis{
        {
            {NodeId(0),
             TextSpan(ByteOffset(0), ByteOffset(8)),
             NodeKind::document,
             {NodeId(1), NodeId(2)},
             {}},
            {NodeId(1),
             TextSpan(ByteOffset(0), ByteOffset(4)),
             NodeKind::unit,
             {},
             {{feature_lexical_core, 1.0F}}},
            {NodeId(2),
             TextSpan(ByteOffset(4), ByteOffset(8)),
             NodeKind::subunit,
             {},
             {{feature_grammatical_affix, 1.0F}}},
        },
        {},
    };
    const auto lexical = LexicalCoreReadingModel().generate(analysis);
    check(lexical.size() == 1, "lexical-core model selects only marked nodes");
    check(lexical[0].span.begin().value() == 0 && lexical[0].span.end().value() == 4,
          "lexical-core model preserves provider span");
    check(near(lexical[0].fixation_salience, 1.0F) && near(lexical[0].lexical_salience, 1.0F),
          "lexical-core model emits normalized reading signals");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core policy tests passed\n";
    return EXIT_SUCCESS;
}
