#ifndef LE_RUNTIME_PLUGIN_PROVIDER_REGISTRY_HPP
#define LE_RUNTIME_PLUGIN_PROVIDER_REGISTRY_HPP

#include "core/ir.hpp"
#include "core/text.hpp"
#include "le/provider.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace le::plugin {

class Error final : public std::runtime_error {
  public:
    Error(le_status_t status, std::string message);
    [[nodiscard]] le_status_t status() const noexcept;

  private:
    le_status_t status_;
};

class ProviderRegistry final {
  public:
    ProviderRegistry();
    ~ProviderRegistry();
    ProviderRegistry(const ProviderRegistry&) = delete;
    ProviderRegistry& operator=(const ProviderRegistry&) = delete;

    void register_provider(const le_provider_v1_t& provider, void* module = nullptr);
    void load(std::string_view path);
    [[nodiscard]] std::optional<core::Analysis> analyze(const core::Text& text,
                                                        std::string_view language) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::string_view name_at(std::size_t index) const;

  private:
    class Entry;
    mutable std::shared_mutex mutex_;
    std::vector<std::unique_ptr<Entry>> entries_;
};

[[nodiscard]] bool dynamic_loading_enabled() noexcept;

} // namespace le::plugin

#endif
