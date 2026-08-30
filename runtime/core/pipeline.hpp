#ifndef LE_RUNTIME_CORE_PIPELINE_HPP
#define LE_RUNTIME_CORE_PIPELINE_HPP

#include "core/emphasis.hpp"
#include "core/ir.hpp"
#include "core/reading.hpp"
#include "core/text.hpp"

#include <string_view>
#include <vector>

namespace le::core {

struct PipelineOptions {
    std::string_view language;
    PrefixModelConfig prefix;
    PresentationConfig presentation;
};

[[nodiscard]] Analysis analyze(const Text& text, std::string_view language);
[[nodiscard]] std::vector<Emphasis> process(const Text& text, const PipelineOptions& options);

} // namespace le::core

#endif
