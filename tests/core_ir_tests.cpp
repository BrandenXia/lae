#include "core/ir.hpp"
#include "core/text.hpp"
#include "providers/generic/generic_provider.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_invalid(const le::core::Text& text, const le::core::Analysis& analysis,
                    std::string_view message) {
    bool rejected = false;
    try {
        le::core::validate_analysis(text, analysis);
    } catch (const le::core::InvalidAnalysis&) {
        rejected = true;
    }
    check(rejected, message);
}

} // namespace

int main() {
    using namespace le::core;

    const Text text("alpha 世界");
    const GenericLanguageProvider provider;
    const auto valid = provider.analyze(text, "zh-Hans");
    validate_analysis(text, valid);
    check(valid.nodes.size() == 3, "generic analysis has document and two units");
    check(valid.nodes.front().children.size() == 2, "document owns both units");
    check(valid.nodes[1].features.size() == 2, "unit has generic features");
    check(valid.language_regions.size() == 1 &&
              valid.language_regions.front().language == "zh-Hans",
          "analysis owns language-region metadata");

    auto bad_id = valid;
    bad_id.nodes[1].id = NodeId(99);
    expect_invalid(text, bad_id, "non-dense node identifier rejected");

    auto bad_feature = valid;
    bad_feature.nodes[1].features.front().value = std::numeric_limits<float>::infinity();
    expect_invalid(text, bad_feature, "non-finite feature rejected");

    auto duplicate_parent = valid;
    duplicate_parent.nodes.front().children.push_back(NodeId(1));
    expect_invalid(text, duplicate_parent, "duplicate parent edge rejected");

    auto bad_region = valid;
    bad_region.language_regions.push_back(bad_region.language_regions.front());
    expect_invalid(text, bad_region, "overlapping language regions rejected");

    auto disconnected_cycle = valid;
    disconnected_cycle.nodes.front().children.clear();
    disconnected_cycle.nodes[1].span = disconnected_cycle.nodes.front().span;
    disconnected_cycle.nodes[2].span = disconnected_cycle.nodes.front().span;
    disconnected_cycle.nodes[1].children = {NodeId(2)};
    disconnected_cycle.nodes[2].children = {NodeId(1)};
    expect_invalid(text, disconnected_cycle, "disconnected child cycle rejected");

    const Text combining("e\u0301");
    auto split_grapheme = provider.analyze(combining, "und");
    split_grapheme.nodes[1].span = TextSpan(ByteOffset(1), ByteOffset(3));
    expect_invalid(combining, split_grapheme, "node that splits grapheme rejected");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core IR tests passed\n";
    return EXIT_SUCCESS;
}
