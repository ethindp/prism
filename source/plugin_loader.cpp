// SPDX-License-Identifier: MPL-2.0

#include "plugin_loader.h"
#include "backend_catalog.h"
#include "logging.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <simdutf.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <tchar.h>
#include <windows.h>
#include <winrt/Windows.Foundation.Diagnostics.h>
#include <winrt/Windows.Foundation.h>
#else
#include <dlfcn.h>
#endif

BackendFactory make_plugin_factory(const PrismBackendVTable *vtable,
                                   void *create_userdata,
                                   std::shared_ptr<void> owner,
                                   std::uint64_t features, std::string name);

namespace {
constexpr std::uint64_t PLUGIN_ABI_MIN_SUPPORTED = UINT64_C(1);

constexpr std::size_t PLUGIN_BACKEND_MIN_SIZE =
    offsetof(PrismPluginBackend, userdata);

struct PluginBackendState;

struct ServicesBlock {
  PrismPluginServices services;
  PluginBackendState *owner;
};
static_assert(std::is_standard_layout_v<ServicesBlock>,
              "ServicesBlock must be standard-layout for container_of");
static_assert(offsetof(ServicesBlock, services) == 0,
              "PrismPluginServices must be the first member of ServicesBlock");

struct PluginBackendState {
  ServicesBlock block;
  PrismPluginInstanceContext ctx;
  std::string name;
};

struct HostBlock {
  PrismPluginHost host;
  const std::string *path;
};
static_assert(std::is_standard_layout_v<HostBlock>);
static_assert(offsetof(HostBlock, host) == 0);

void submit(PrismLogLevel level, const std::string &source,
            const char *message) noexcept {
  Logger &lg = logger();
  if (!lg.wants(level))
    return;
  // NOLINTBEGIN(bugprone-empty-catch)
  try {
    lg.submit(level, source, std::string{message});
  } catch (...) {
  }
  // NOLINTEND(bugprone-empty-catch)
}

void PRISM_CALL plugin_log(const PrismPluginServices *self, PrismLogLevel level,
                           const char *message) {
  if (self == nullptr || message == nullptr)
    return;
  const auto *block = reinterpret_cast<const ServicesBlock *>(self);
  submit(level, block->owner->name, message);
}

void PRISM_CALL host_log(const PrismPluginHost *self, PrismLogLevel level,
                         const char *message) {
  if (self == nullptr || message == nullptr)
    return;
  const auto *block = reinterpret_cast<const HostBlock *>(self);
  submit(level, *block->path, message);
}

constexpr std::size_t PLUGIN_MAX_BACKENDS = 256;
constexpr std::size_t PLUGIN_NAME_MAX = 4096;

#ifdef _WIN32
using LibraryHandle = HMODULE;

LibraryHandle open_library(std::string_view path) {
  std::u16string wide(simdutf::utf16_length_from_utf8(path.data(), path.size()),
                      u'\0');
  if (simdutf::convert_utf8_to_utf16le(path.data(), path.size(), wide.data()) ==
      0) {
    SetLastError(ERROR_NO_UNICODE_TRANSLATION);
    return nullptr;
  }
  const auto *raw = reinterpret_cast<LPCWSTR>(wide.c_str());
  const DWORD needed = GetFullPathName(raw, 0, nullptr, nullptr);
  if (needed == 0)
    return nullptr;
  std::wstring full(needed, _T('\0'));
  const DWORD written = GetFullPathName(raw, needed, full.data(), nullptr);
  if (written == 0 || written >= needed)
    return nullptr;
  full.resize(written);
  return LoadLibraryEx(full.c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
}

void close_library(LibraryHandle h) { FreeLibrary(h); }

std::string last_library_error() {
  const auto d =
      winrt::Windows::Foundation::Diagnostics::ErrorDetails::
          CreateFromHResultAsync(static_cast<HRESULT>(GetLastError()))
              .get();
  return winrt::to_string(d.Description());
}

PrismPluginQueryFn resolve_entry(LibraryHandle h) noexcept {
  return reinterpret_cast<PrismPluginQueryFn>(
      GetProcAddress(h, "prism_plugin_query"));
}
#else
using LibraryHandle = void *;

LibraryHandle open_library(std::string_view path) {
  const std::string owned{path};
  return dlopen(owned.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void close_library(LibraryHandle h) { dlclose(h); }

std::string last_library_error() {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char *e = dlerror();
  return e != nullptr ? std::string{e} : std::string{"unknown error"};
}

PrismPluginQueryFn resolve_entry(LibraryHandle h) noexcept {
  void *sym = dlsym(h, "prism_plugin_query");
  if (sym == nullptr)
    return nullptr;
  return reinterpret_cast<PrismPluginQueryFn>(sym);
}
#endif

struct LoadedLibrary {
  LibraryHandle handle{nullptr};
  std::vector<std::unique_ptr<PluginBackendState>> backends;

  LoadedLibrary() = default;
  LoadedLibrary(const LoadedLibrary &) = delete;
  LoadedLibrary &operator=(const LoadedLibrary &) = delete;
  ~LoadedLibrary() {
    if (handle != nullptr)
      close_library(handle);
  }
};

std::size_t bounded_length(const char *s, const std::size_t max) noexcept {
#ifdef _WIN32
  return strnlen_s(s, max);
#else
  std::size_t n = 0;
  while (n < max && s[n] != '\0')
    ++n;
  return n;
#endif
}
} // namespace

PrismError load_plugin(RegistryBuilder &builder, const char *path,
                       int priority_override, std::size_t *out_count) {
  static const LogSource log{"PluginLoader"};
  if (path == nullptr)
    return PRISM_ERROR_INVALID_PARAM;
  if (builder.spent()) {
    log.error("Refusing to load '{}': the builder is spent", path);
    return PRISM_ERROR_INVALID_OPERATION;
  }
  const std::size_t mark = builder.count();
  try {
    const std::string_view path_view{path};
    if (!simdutf::validate_utf8(path_view.data(), path_view.size())) {
      log.error("Refusing to load a plugin: the path is not valid UTF-8");
      return PRISM_ERROR_INVALID_UTF8;
    }
    const std::string path_owned{path_view};
    auto library = std::make_shared<LoadedLibrary>();
    library->handle = open_library(path_view);
    if (library->handle == nullptr) {
      log.error("Failed to open '{}': {}", path_owned, last_library_error());
      return PRISM_ERROR_LIBRARY_LOAD_FAILED;
    }
    auto *const query = resolve_entry(library->handle);
    if (query == nullptr) {
      log.error("'{}' does not export prism_plugin_query: {}", path_owned,
                last_library_error());
      return PRISM_ERROR_LIBRARY_INVALID;
    }
    HostBlock host{};
    host.host.abi_version = PRISM_PLUGIN_ABI_VERSION;
    host.host.struct_size = static_cast<std::uint32_t>(sizeof(PrismPluginHost));
    host.host.reserved = 0;
    host.host.log = &host_log;
    host.path = &path_owned;
    std::size_t added = 0;
    for (std::size_t index = 0;; ++index) {
      if (index >= PLUGIN_MAX_BACKENDS) {
        log.error("'{}' supplied more than {} backends; refusing the plugin",
                  path_owned, PLUGIN_MAX_BACKENDS);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      const PrismPluginBackend *raw = query(&host.host, index);
      if (raw == nullptr)
        break;
      if (raw->abi_version < PLUGIN_ABI_MIN_SUPPORTED ||
          raw->abi_version > PRISM_PLUGIN_ABI_VERSION) {
        log.error("'{}' backend {} declares plugin ABI generation {}, but this "
                  " build accepts {} through {}",
                  path_owned, index, raw->abi_version, PLUGIN_ABI_MIN_SUPPORTED,
                  PRISM_PLUGIN_ABI_VERSION);
        builder.rollback_to(mark);
        return PRISM_ERROR_INCOMPATIBLE_ABI;
      }
      if (raw->struct_size < PLUGIN_BACKEND_MIN_SIZE) {
        log.error("'{}' backend {} declares struct_size {}; at least {} bytes "
                  "are required",
                  path_owned, index, raw->struct_size, PLUGIN_BACKEND_MIN_SIZE);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      PrismPluginBackend d{};
      std::memcpy(
          &d, raw,
          std::min<std::size_t>(raw->struct_size, sizeof(PrismPluginBackend)));
      if (d.reserved != 0) {
        log.error("'{}' backend {} has a non-zero reserved member", path_owned,
                  index);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      if (d.name == nullptr) {
        log.error("'{}' backend {} declares a null name", path_owned, index);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      const std::size_t name_len = bounded_length(d.name, PLUGIN_NAME_MAX);
      if (name_len == 0 || name_len == PLUGIN_NAME_MAX) {
        log.error("'{}' backend {} declares an empty or unterminated name",
                  path_owned, index);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      const std::string_view name_view{d.name, name_len};
      if (!simdutf::validate_utf8(name_view.data(), name_view.size())) {
        log.error("'{}' backend {} declares a name that is not valid UTF-8",
                  path_owned, index);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_UTF8;
      }
      if (d.vtable == nullptr || d.vtable->size == 0) {
        log.error("'{}' backend '{}' declares a null or empty vtable",
                  path_owned, name_view);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      const int priority =
          priority_override >= 0 ? priority_override : d.priority;
      if (priority < 0) {
        log.error("'{}' backend '{}' declares priority {} and no override was "
                  "supplied",
                  path_owned, name_view, d.priority);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      auto state = std::make_unique<PluginBackendState>();
      state->name = std::string{name_view};
      state->block.services.struct_size =
          static_cast<std::uint32_t>(sizeof(PrismPluginServices));
      state->block.services.reserved = 0;
      state->block.services.log = &plugin_log;
      state->block.owner = state.get();
      state->ctx.struct_size =
          static_cast<std::uint32_t>(sizeof(PrismPluginInstanceContext));
      state->ctx.reserved = 0;
      state->ctx.services = &state->block.services;
      state->ctx.userdata = d.userdata;
      auto *ctx_ptr = &state->ctx;
      library->backends.push_back(std::move(state));
      auto factory = make_plugin_factory(d.vtable, ctx_ptr, library, d.features,
                                         std::string{name_view});
      if (!factory) {
        log.error("'{}' backend '{}' has no create member, or declares a "
                  "feature set inconsistent with its vtable",
                  path_owned, name_view);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      }
      BackendId id{};
      switch (builder.add(std::string{name_view}, priority, std::move(factory),
                          &id)) {
      case BuilderResult::Ok:
        break;
      case BuilderResult::InvalidUtf8: {
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_UTF8;
      } break;
      case BuilderResult::EmptyName:
      case BuilderResult::NegativePriority:
      case BuilderResult::ReservedId: {
        log.error("'{}' backend '{}' was rejected by the builder", path_owned,
                  name_view);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_PARAM;
      } break;
      case BuilderResult::Spent:
      case BuilderResult::DuplicateName:
      case BuilderResult::DuplicateId: {
        log.error("'{}' backend '{}' collides with a backend already present "
                  "in the builder",
                  path_owned, name_view);
        builder.rollback_to(mark);
        return PRISM_ERROR_INVALID_OPERATION;
      } break;
      }
      log.debug("'{}' registered backend '{}' (priority {}, plugin version "
                "{}.{}.{})",
                path_owned, name_view, priority,
                static_cast<std::uint16_t>(d.plugin_version >> 48),
                static_cast<std::uint16_t>(d.plugin_version >> 32),
                static_cast<std::uint16_t>(d.plugin_version >> 16));
      ++added;
    }
    if (added == 0) {
      log.info("'{}' supplied no backends", path_owned);
      builder.rollback_to(mark);
      return PRISM_ERROR_INCOMPATIBLE_ABI;
    }
    if (out_count != nullptr)
      *out_count = added;
    return PRISM_OK;
  } catch (...) {
    builder.rollback_to(mark);
    log.error("Failed to load '{}': out of memory", path);
    return PRISM_ERROR_MEMORY_FAILURE;
  }
}
