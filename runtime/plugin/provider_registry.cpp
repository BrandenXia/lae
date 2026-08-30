#include "plugin/provider_registry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>

#if defined(LE_DYNAMIC_PROVIDER_LOADING) && defined(_WIN32)
#include <windows.h>
#elif defined(LE_DYNAMIC_PROVIDER_LOADING)
#include <dlfcn.h>
#endif

namespace le::plugin {
namespace {

constexpr std::uint32_t supported_flags = LE_PROVIDER_FLAG_THREAD_SAFE;

bool valid_view(le_string_view_t view) { return view.data != nullptr || view.size == 0; }

bool valid_language(le_string_view_t language) {
    if (!valid_view(language) || language.size == 0 || language.size > 255) {
        return false;
    }
    bool previous_hyphen = true;
    for (std::size_t index = 0; index < language.size; ++index) {
        const auto character = static_cast<unsigned char>(language.data[index]);
        const bool hyphen = character == '-';
        const bool alphanumeric = (character >= 'a' && character <= 'z') ||
                                  (character >= 'A' && character <= 'Z') ||
                                  (character >= '0' && character <= '9');
        if ((!hyphen && !alphanumeric) || (hyphen && previous_hyphen)) {
            return false;
        }
        previous_hyphen = hyphen;
    }
    return !previous_hyphen;
}

core::NodeKind node_kind(le_node_kind_t kind) {
    switch (kind) {
    case LE_NODE_DOCUMENT:
        return core::NodeKind::document;
    case LE_NODE_BLOCK:
        return core::NodeKind::block;
    case LE_NODE_PARAGRAPH:
        return core::NodeKind::paragraph;
    case LE_NODE_SENTENCE:
        return core::NodeKind::sentence;
    case LE_NODE_UNIT:
        return core::NodeKind::unit;
    case LE_NODE_SUBUNIT:
        return core::NodeKind::subunit;
    default:
        throw std::invalid_argument("provider emitted an unknown node kind");
    }
}

struct Builder {
    core::Analysis analysis;
    le_status_t status = LE_OK;
    const char* error = nullptr;
};

template <typename Function> le_status_t build_event(void* context, Function&& function) noexcept {
    if (context == nullptr) {
        return LE_ERROR_PLUGIN_FAILURE;
    }
    auto& builder = *static_cast<Builder*>(context);
    if (builder.status != LE_OK) {
        return builder.status;
    }
    try {
        function(builder);
        return LE_OK;
    } catch (const std::bad_alloc&) {
        builder.status = LE_ERROR_OUT_OF_MEMORY;
        builder.error = "provider analysis allocation failed";
    } catch (const std::exception&) {
        builder.status = LE_ERROR_PLUGIN_FAILURE;
        builder.error = "provider emitted an invalid analysis event";
    } catch (...) {
        builder.status = LE_ERROR_PLUGIN_FAILURE;
        builder.error = "provider emitted an invalid analysis event";
    }
    return builder.status;
}

le_status_t add_node(void* context, le_node_id_t id, le_node_kind_t kind,
                     le_text_span_t span) noexcept {
    return build_event(context, [&](Builder& builder) {
        if (id != builder.analysis.nodes.size()) {
            throw std::invalid_argument("provider node identifiers must be dense and ordered");
        }
        builder.analysis.nodes.push_back(core::Node{
            core::NodeId(id),
            core::TextSpan(core::ByteOffset(span.begin), core::ByteOffset(span.end)),
            node_kind(kind),
            {},
            {},
        });
    });
}

le_status_t add_child(void* context, le_node_id_t parent, le_node_id_t child) noexcept {
    return build_event(context, [&](Builder& builder) {
        if (parent >= builder.analysis.nodes.size() || child >= builder.analysis.nodes.size()) {
            throw std::invalid_argument("provider child event references an unknown node");
        }
        builder.analysis.nodes[parent].children.push_back(core::NodeId(child));
    });
}

le_status_t add_feature(void* context, le_node_id_t node, le_feature_id_t feature,
                        float value) noexcept {
    return build_event(context, [&](Builder& builder) {
        if (node >= builder.analysis.nodes.size()) {
            throw std::invalid_argument("provider feature event references an unknown node");
        }
        if (!std::isfinite(value)) {
            throw std::invalid_argument("provider feature value is not finite");
        }
        builder.analysis.nodes[node].features.push_back(core::Feature{feature, value});
    });
}

le_status_t add_language_region(void* context, le_text_span_t span, le_string_view_t language,
                                float confidence) noexcept {
    return build_event(context, [&](Builder& builder) {
        if (!valid_language(language)) {
            throw std::invalid_argument("provider emitted an invalid language tag");
        }
        if (!std::isfinite(confidence)) {
            throw std::invalid_argument("provider language confidence is not finite");
        }
        builder.analysis.language_regions.push_back(core::LanguageRegion{
            core::TextSpan(core::ByteOffset(span.begin), core::ByteOffset(span.end)),
            std::string(language.data, language.size),
            confidence,
        });
    });
}

void close_module(void* module) noexcept {
    if (module == nullptr) {
        return;
    }
#if defined(LE_DYNAMIC_PROVIDER_LOADING) && defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(module));
#elif defined(LE_DYNAMIC_PROVIDER_LOADING)
    dlclose(module);
#else
    static_cast<void>(module);
#endif
}

struct OpenedProvider {
    void* module;
    const le_provider_v1_t* descriptor;
};

OpenedProvider open_provider(std::string_view path) {
#if defined(LE_DYNAMIC_PROVIDER_LOADING) && defined(_WIN32)
    const std::string owned_path(path);
    auto* module = LoadLibraryA(owned_path.c_str());
    if (module == nullptr) {
        throw Error(LE_ERROR_PLUGIN_FAILURE, "could not load provider module");
    }
    const auto symbol = GetProcAddress(module, LE_PROVIDER_ENTRY_V1_NAME);
    if (symbol == nullptr) {
        close_module(module);
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider entry point is missing");
    }
    const auto entry = reinterpret_cast<le_provider_entry_v1_fn>(symbol);
#elif defined(LE_DYNAMIC_PROVIDER_LOADING)
    const std::string owned_path(path);
    void* module = dlopen(owned_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) {
        const char* detail = dlerror();
        throw Error(LE_ERROR_PLUGIN_FAILURE,
                    detail == nullptr ? "could not load provider module" : detail);
    }
    dlerror();
    void* symbol = dlsym(module, LE_PROVIDER_ENTRY_V1_NAME);
    const char* symbol_error = dlerror();
    if (symbol_error != nullptr || symbol == nullptr) {
        close_module(module);
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider entry point is missing");
    }
    le_provider_entry_v1_fn entry = nullptr;
    static_assert(sizeof(entry) == sizeof(symbol));
    std::memcpy(&entry, &symbol, sizeof(entry));
#else
    static_cast<void>(path);
    throw Error(LE_ERROR_UNSUPPORTED, "dynamic provider loading is disabled in this build");
#endif
#if defined(LE_DYNAMIC_PROVIDER_LOADING)
    const le_provider_v1_t* descriptor = nullptr;
    try {
        descriptor = entry();
    } catch (...) {
        close_module(module);
        throw Error(LE_ERROR_PLUGIN_FAILURE, "provider entry point threw an exception");
    }
    if (descriptor == nullptr) {
        close_module(module);
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider entry point returned null");
    }
    return OpenedProvider{module, descriptor};
#endif
}

} // namespace

Error::Error(le_status_t status, std::string message)
    : std::runtime_error(std::move(message)), status_(status) {}

le_status_t Error::status() const noexcept { return status_; }

class ProviderRegistry::Entry final {
  public:
    Entry(const le_provider_v1_t& source, std::string provider_name, void* provider_module)
        : provider(source), name(std::move(provider_name)), module(provider_module) {
        provider.name = le_string_view_t{name.data(), name.size()};
    }

    ~Entry() {
        if (active && provider.destroy != nullptr) {
            try {
                provider.destroy(provider.context);
            } catch (...) {
                // Destruction cannot report a status; plugin exceptions stop here.
            }
        }
        close_module(module);
    }

    void activate(void* provider_module) noexcept {
        module = provider_module;
        active = true;
    }

    le_provider_v1_t provider;
    std::string name;
    void* module;
    bool active = false;
    mutable std::mutex call_mutex;
};

ProviderRegistry::ProviderRegistry() = default;
ProviderRegistry::~ProviderRegistry() = default;

void ProviderRegistry::register_provider(const le_provider_v1_t& provider, void* module) {
    if (provider.struct_size < LE_PROVIDER_V1_SIZE) {
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider descriptor is smaller than v1");
    }
    if (provider.abi_version != LE_PROVIDER_ABI_VERSION) {
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider ABI version is incompatible");
    }
    if ((provider.flags & ~supported_flags) != 0 || provider.reserved != 0) {
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider descriptor uses unsupported fields");
    }
    if (!valid_view(provider.name) || provider.name.size == 0 || provider.name.size > 255) {
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider name is invalid");
    }
    if (provider.supports == nullptr || provider.analyze == nullptr) {
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider callbacks are incomplete");
    }

    std::string name(provider.name.data, provider.name.size);
    std::unique_lock lock(mutex_);
    const auto duplicate = std::ranges::find_if(
        entries_, [&](const auto& existing) { return existing->name == name; });
    if (duplicate != entries_.end()) {
        throw Error(LE_ERROR_PLUGIN_INCOMPATIBLE, "provider name is already registered");
    }
    auto entry = std::make_unique<Entry>(provider, name, nullptr);
    entries_.push_back(std::move(entry));
    entries_.back()->activate(module);
}

void ProviderRegistry::load(std::string_view path) {
    if (path.empty()) {
        throw Error(LE_ERROR_INVALID_ARGUMENT, "provider module path is empty");
    }
    if (path.find('\0') != std::string_view::npos) {
        throw Error(LE_ERROR_INVALID_ARGUMENT, "provider module path contains a null byte");
    }
    auto opened = open_provider(path);
    try {
        register_provider(*opened.descriptor, opened.module);
    } catch (...) {
        close_module(opened.module);
        throw;
    }
}

std::optional<core::Analysis> ProviderRegistry::analyze(const core::Text& text,
                                                        std::string_view language) const {
    std::shared_lock registry_lock(mutex_);
    for (const auto& entry : entries_) {
        const le_string_view_t language_view{language.data(), language.size()};
        std::unique_lock<std::mutex> provider_lock(entry->call_mutex, std::defer_lock);
        if ((entry->provider.flags & LE_PROVIDER_FLAG_THREAD_SAFE) == 0) {
            provider_lock.lock();
        }
        int supported = 0;
        try {
            supported = entry->provider.supports(entry->provider.context, language_view);
        } catch (...) {
            throw Error(LE_ERROR_PLUGIN_FAILURE, "provider supports callback threw an exception");
        }
        if (supported == 0) {
            continue;
        }

        Builder builder;
        const le_analysis_sink_v1_t sink{
            LE_ANALYSIS_SINK_V1_SIZE, 0, &builder, add_node, add_child, add_feature,
            add_language_region,
        };
        le_status_t status = LE_OK;
        try {
            status = entry->provider.analyze(
                entry->provider.context, le_string_view_t{text.bytes().data(), text.bytes().size()},
                language_view, &sink);
        } catch (const std::bad_alloc&) {
            throw Error(LE_ERROR_OUT_OF_MEMORY, "provider analysis allocation failed");
        } catch (...) {
            throw Error(LE_ERROR_PLUGIN_FAILURE, "provider analysis callback threw an exception");
        }
        if (builder.status != LE_OK) {
            throw Error(builder.status,
                        builder.error == nullptr ? "provider sink failed" : builder.error);
        }
        if (status != LE_OK) {
            throw Error(status == LE_ERROR_OUT_OF_MEMORY ? status : LE_ERROR_PLUGIN_FAILURE,
                        "provider analysis callback failed");
        }
        try {
            core::validate_analysis(text, builder.analysis);
        } catch (const core::InvalidAnalysis& error) {
            throw Error(LE_ERROR_PLUGIN_FAILURE, error.what());
        }
        return std::move(builder.analysis);
    }
    return std::nullopt;
}

std::size_t ProviderRegistry::size() const {
    std::shared_lock lock(mutex_);
    return entries_.size();
}

std::string_view ProviderRegistry::name_at(std::size_t index) const {
    std::shared_lock lock(mutex_);
    return index < entries_.size() ? std::string_view(entries_[index]->name) : std::string_view{};
}

bool dynamic_loading_enabled() noexcept {
#if defined(LE_DYNAMIC_PROVIDER_LOADING)
    return true;
#else
    return false;
#endif
}

} // namespace le::plugin
