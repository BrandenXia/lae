#ifndef LE_RUNTIME_CORE_PIPELINE_HPP
#define LE_RUNTIME_CORE_PIPELINE_HPP

#include "core/text.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace le::core {

class NodeId {
  public:
    constexpr explicit NodeId(std::uint32_t value) : value_(value) {}
    [[nodiscard]] constexpr std::uint32_t value() const { return value_; }

  private:
    std::uint32_t value_;
};

enum class NodeKind : std::uint8_t {
    document,
    unit,
    subunit,
};

using FeatureId = std::uint32_t;

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
    std::string_view language;
    float confidence;
};

struct Analysis {
    std::vector<Node> nodes;
    std::vector<LanguageRegion> language_regions;
};

class LanguageProvider {
  public:
    virtual ~LanguageProvider() = default;
    [[nodiscard]] virtual Analysis analyze(const Text& text, std::string_view language) const = 0;
};

class GenericLanguageProvider final : public LanguageProvider {
  public:
    [[nodiscard]] Analysis analyze(const Text& text, std::string_view language) const override;
};

struct ReadingSignal {
    TextSpan span;
    float fixation_salience;
    float lexical_salience;
    float reading_difficulty;
};

enum class PrefixStrategy : std::uint8_t {
    proportional,
    fixed,
};

struct PrefixModelConfig {
    PrefixStrategy strategy;
    std::uint32_t fixed_graphemes;
    float proportion;
};

class PrefixReadingModel {
  public:
    explicit PrefixReadingModel(PrefixModelConfig config) : config_(config) {}
    [[nodiscard]] std::vector<ReadingSignal> generate(const Text& text,
                                                      const Analysis& analysis) const;

  private:
    PrefixModelConfig config_;
};

struct Emphasis {
    TextSpan span;
    float strength;
    std::uint32_t style_class;
};

class BinaryBoldPolicy {
  public:
    explicit BinaryBoldPolicy(float strength) : strength_(strength) {}
    [[nodiscard]] std::vector<Emphasis> apply(const std::vector<ReadingSignal>& signals) const;

  private:
    float strength_;
};

struct PipelineOptions {
    std::string_view language;
    PrefixModelConfig prefix;
    float emphasis_strength;
};

[[nodiscard]] std::vector<Emphasis> process(const Text& text, const PipelineOptions& options);

} // namespace le::core

#endif
