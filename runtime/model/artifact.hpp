#ifndef LE_RUNTIME_MODEL_ARTIFACT_HPP
#define LE_RUNTIME_MODEL_ARTIFACT_HPP

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace le::model {

inline constexpr std::size_t header_size = 64;
inline constexpr std::size_t maximum_artifact_size = 16 * 1024 * 1024;

enum class ErrorKind : std::uint8_t {
    invalid,
    incompatible,
};

class ArtifactError final : public std::runtime_error {
  public:
    ArtifactError(ErrorKind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}
    [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }

  private:
    ErrorKind kind_;
};

struct Artifact {
    std::uint32_t minimum_abi_version;
    std::uint32_t type;
    std::uint32_t model_version;
    std::vector<std::string> languages;
    std::vector<std::uint32_t> required_features;
    std::uint32_t prefix_strategy;
    std::uint32_t fixed_graphemes;
    float prefix_proportion;
};

[[nodiscard]] Artifact load(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> encode(const Artifact& artifact);
[[nodiscard]] bool supports_language(const Artifact& artifact, std::string_view language);
[[nodiscard]] std::uint32_t checksum(std::span<const std::uint8_t> bytes);

} // namespace le::model

#endif
