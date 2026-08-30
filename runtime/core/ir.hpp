#ifndef LE_RUNTIME_CORE_IR_HPP
#define LE_RUNTIME_CORE_IR_HPP

#include "core/text.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace le::core {

class NodeId {
  public:
    constexpr explicit NodeId(std::uint32_t value) : value_(value) {}
    [[nodiscard]] constexpr std::uint32_t value() const { return value_; }
    friend constexpr bool operator==(NodeId, NodeId) = default;

  private:
    std::uint32_t value_;
};

enum class NodeKind : std::uint8_t {
    document,
    block,
    paragraph,
    sentence,
    unit,
    subunit,
};

using FeatureId = std::uint32_t;
inline constexpr FeatureId feature_boundary_strength = 0x00000001u;
inline constexpr FeatureId feature_grapheme_count = 0x00000002u;
inline constexpr FeatureId feature_segmentation_confidence = 0x00000003u;
inline constexpr FeatureId feature_lexical_core = 0x00010001u;
inline constexpr FeatureId feature_derivational_affix = 0x00010002u;
inline constexpr FeatureId feature_grammatical_affix = 0x00010003u;
inline constexpr FeatureId feature_content_unit = 0x00030001u;
inline constexpr FeatureId feature_script_han = 0x00040001u;
inline constexpr FeatureId feature_script_latin = 0x00040002u;

struct Feature {
    FeatureId id;
    float value;
};

struct Node {
    NodeId id;
    TextSpan span;
    NodeKind kind;
    std::vector<NodeId> children;
    std::vector<Feature> features;
};

struct LanguageRegion {
    TextSpan span;
    std::string language;
    float confidence;
};

struct Analysis {
    std::vector<Node> nodes;
    std::vector<LanguageRegion> language_regions;
};

class InvalidAnalysis final : public std::logic_error {
  public:
    using std::logic_error::logic_error;
};

void validate_analysis(const Text& text, const Analysis& analysis);

} // namespace le::core

#endif
