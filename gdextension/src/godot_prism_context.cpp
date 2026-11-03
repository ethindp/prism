#include "godot_prism_context.h"

#include "godot_cpp/classes/project_settings.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

#include <atomic>
#include <bit>
#include <cassert>
#include <limits>
#include <utility>

static std::atomic<std::uint64_t> log_signal_instance{0};

static inline int64_t id_to_godot(PrismBackendId id) {
  return std::bit_cast<std::int64_t>(id);
}

static inline PrismBackendId id_from_godot(int64_t v) {
  return std::bit_cast<std::uint64_t>(v);
}

Ref<GodotPrismBackend> GodotPrismContext::_wrap(PrismBackend *raw) {
  if (raw == nullptr) {
    return {};
  }
  Ref<GodotPrismBackend> ref;
  ref.instantiate();
  ref->_init_raw(raw);
  return ref;
}

GodotPrismContext::GodotPrismContext() {
  PrismConfig cfg = prism_config_init();
  ctx = prism_init(&cfg);
  if (ctx == nullptr) {
    UtilityFunctions::push_error(
        "Prism: context creation failed (out of memory?)");
  }
}

GodotPrismContext::~GodotPrismContext() {
  set_log_signal_enabled(false);
  stop_availability_monitoring();
  if (ctx != nullptr) {
    prism_shutdown(ctx);
    ctx = nullptr;
  }
  if (registry != nullptr) {
    prism_registry_release(registry);
    registry = nullptr;
  }
}

void PRISM_CALL GodotPrismContext::on_availability_changed(
    void *userdata, PrismBackendId backend, const char *name, bool available) {
  static_cast<GodotPrismContext *>(userdata)->call_deferred(
      "emit_signal", "backend_availability_changed", id_to_godot(backend),
      String::utf8(name), available);
}

void PRISM_CALL GodotPrismContext::on_availability_baseline(void *userdata) {
  static_cast<GodotPrismContext *>(userdata)->call_deferred(
      "emit_signal", "availability_baseline_established");
}

void PRISM_CALL GodotPrismContext::on_log_message(
    [[maybe_unused]] void *userdata, PrismLogLevel level, const char *source,
    const char *message) {
  auto const instance_id = log_signal_instance.load();
  if (instance_id == 0) {
    return;
  }
  auto *const instance = ObjectDB::get_instance(instance_id);
  if (instance != nullptr) {
    instance->call_deferred("emit_signal", "log_message_received", level,
                            String::utf8(source), String::utf8(message));
  }
}

PrismError GodotPrismContext::open_monitor_context() {
  PrismConfig cfg = prism_config_init();
  cfg.registry = registry;
  cfg.availability_callback = &GodotPrismContext::on_availability_changed;
  cfg.availability_baseline_callback =
      &GodotPrismContext::on_availability_baseline;
  cfg.availability_userdata = this;
  cfg.availability_poll_interval_ms = monitoring_options.poll_interval_ms;
  cfg.availability_debounce_samples = monitoring_options.debounce_samples;
  cfg.availability_backoff_max_ms = monitoring_options.backoff_max_ms;
  cfg.availability_auto_power_manage = monitoring_options.auto_power_manage;
  monitor_ctx = prism_init(&cfg);
  return monitor_ctx != nullptr ? PRISM_OK : PRISM_ERROR_INTERNAL;
}

bool GodotPrismContext::is_backend_available(std::int64_t id) {
  if (ctx == nullptr) {
    UtilityFunctions::push_error(
        "Prism: called is_backend_available on a null context");
    return false;
  }
  auto *const candidate = prism_registry_create(ctx, id_from_godot(id));
  if (candidate == nullptr) {
    return false;
  }
  auto const features = prism_backend_get_features(candidate);
  prism_backend_free(candidate);
  return (features & PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0;
}

PrismError
GodotPrismContext::load_backend_library(const String &path,
                                        std::int64_t priority_override) {
  if (path.is_empty() || priority_override > std::numeric_limits<int>::max()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  auto const global_path =
      ProjectSettings::get_singleton()->globalize_path(path);
  std::vector<BackendLibrary> libraries = backend_libraries;
  libraries.push_back(BackendLibrary{
      .path = std::string(global_path.utf8().get_data()),
      .priority_override =
          priority_override < 0 ? -1 : static_cast<int>(priority_override),
  });
  auto *const builder = prism_registry_builder_new();
  if (builder == nullptr) {
    return PRISM_ERROR_MEMORY_FAILURE;
  }
  for (auto const &library : libraries) {
    auto const err = prism_registry_builder_add_library(
        builder, library.path.c_str(), library.priority_override, nullptr);
    if (err != PRISM_OK) {
      prism_registry_builder_free(builder);
      return err;
    }
  }
  auto *const next_registry = prism_registry_freeze(builder);
  prism_registry_builder_free(builder);
  if (next_registry == nullptr) {
    return PRISM_ERROR_MEMORY_FAILURE;
  }
  PrismConfig cfg = prism_config_init();
  cfg.registry = next_registry;
  auto *const next_ctx = prism_init(&cfg);
  if (next_ctx == nullptr) {
    prism_registry_release(next_registry);
    return PRISM_ERROR_INTERNAL;
  }
  auto const was_monitoring = monitor_ctx != nullptr;
  stop_availability_monitoring();
  if (ctx != nullptr) {
    prism_shutdown(ctx);
  }
  if (registry != nullptr) {
    prism_registry_release(registry);
  }
  ctx = next_ctx;
  registry = next_registry;
  backend_libraries = std::move(libraries);
  return was_monitoring ? open_monitor_context() : PRISM_OK;
}

PrismError GodotPrismContext::start_availability_monitoring(
    std::int64_t poll_interval_ms, std::int64_t debounce_samples,
    std::int64_t backoff_max_ms, bool auto_power_manage) {
  constexpr std::int64_t limit = std::numeric_limits<std::uint32_t>::max();
  if (poll_interval_ms < 0 || poll_interval_ms > limit ||
      debounce_samples < 0 || debounce_samples > limit || backoff_max_ms < 0 ||
      backoff_max_ms > limit) {
    return PRISM_ERROR_RANGE_OUT_OF_BOUNDS;
  }
  if (monitor_ctx != nullptr) {
    return PRISM_ERROR_INVALID_OPERATION;
  }
  monitoring_options = MonitoringOptions{
      .poll_interval_ms = static_cast<std::uint32_t>(poll_interval_ms),
      .debounce_samples = static_cast<std::uint32_t>(debounce_samples),
      .backoff_max_ms = static_cast<std::uint32_t>(backoff_max_ms),
      .auto_power_manage = auto_power_manage,
  };
  return open_monitor_context();
}

void GodotPrismContext::stop_availability_monitoring() {
  if (monitor_ctx != nullptr) {
    prism_shutdown(monitor_ctx);
    monitor_ctx = nullptr;
  }
}

void GodotPrismContext::pause_availability_monitoring() {
  if (monitor_ctx != nullptr) {
    prism_availability_poll_pause(monitor_ctx);
  }
}

void GodotPrismContext::resume_availability_monitoring() {
  if (monitor_ctx != nullptr) {
    prism_availability_poll_resume(monitor_ctx);
  }
}

bool GodotPrismContext::is_availability_monitoring() const {
  return monitor_ctx != nullptr;
}

bool GodotPrismContext::is_auto_power_management_supported() {
  return prism_availability_auto_power_supported();
}

void GodotPrismContext::set_log_signal_enabled(bool enabled) {
  if (enabled == log_signal_enabled) {
    return;
  }
  if (enabled) {
    log_signal_instance.store(get_instance_id());
    previous_log_handler = prism_set_log_handler(PrismLogHandler{
        .fn = &GodotPrismContext::on_log_message,
        .userdata = nullptr,
    });
  } else {
    prism_log_flush();
    log_signal_instance.store(0);
    prism_set_log_handler(previous_log_handler);
    previous_log_handler = PrismLogHandler{};
  }
  log_signal_enabled = enabled;
}

bool GodotPrismContext::is_log_signal_enabled() const {
  return log_signal_enabled;
}

PrismLogLevel GodotPrismContext::set_log_level(PrismLogLevel level) {
  return prism_set_log_level(level);
}

PrismError GodotPrismContext::write_log(PrismLogLevel level,
                                        const String &source,
                                        const String &message) {
  if (source.is_empty() || message.is_empty()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  prism_log(level, source.utf8().get_data(), message.utf8().get_data());
  return PRISM_OK;
}

void GodotPrismContext::flush_log() { prism_log_flush(); }

String GodotPrismContext::get_error_string(PrismError error) {
  return String::utf8(prism_error_string(error));
}

std::int64_t GodotPrismContext::get_version() {
  return static_cast<std::int64_t>(prism_version());
}

String GodotPrismContext::get_version_string() {
  return String::utf8(prism_version_string());
}

std::int64_t GodotPrismContext::get_backends_count() const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error(
        "Prism: called get_backends_count on a null context");
    return 0;
  }
  auto const n = prism_registry_count(ctx);
  return static_cast<std::int64_t>(n);
}

std::int64_t GodotPrismContext::id_at(std::int64_t index) const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called id_at on a null context");
    return id_to_godot(PRISM_BACKEND_INVALID);
  }
  if (index < 0) {
    return id_to_godot(PRISM_BACKEND_INVALID);
  }
  return id_to_godot(
      prism_registry_id_at(ctx, static_cast<std::size_t>(index)));
}

std::int64_t GodotPrismContext::id_of(const String &name) const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called id_of on a null context");
    return id_to_godot(PRISM_BACKEND_INVALID);
  }
  if (name.is_empty()) {
    return id_to_godot(PRISM_BACKEND_INVALID);
  }
  return id_to_godot(prism_registry_id(ctx, name.utf8().get_data()));
}

String GodotPrismContext::name_of(std::int64_t id) const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called name_of on a null context");
    return {};
  }
  auto const bid = id_from_godot(id);
  if (bid == PRISM_BACKEND_INVALID) {
    return {};
  }
  return String::utf8(prism_registry_name(ctx, bid));
}

std::int64_t GodotPrismContext::priority_of(std::int64_t id) const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called priority_of on a null context");
    return -1;
  }
  return prism_registry_priority(ctx, id_from_godot(id));
}

bool GodotPrismContext::has(std::int64_t id) const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called has on a null context");
    return false;
  }
  return prism_registry_exists(ctx, id_from_godot(id));
}

Ref<GodotPrismBackend> GodotPrismContext::get_backend(std::int64_t id) {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called get_backend on a null context");
    return {};
  }
  return _wrap(prism_registry_get(ctx, id_from_godot(id)));
}

Ref<GodotPrismBackend> GodotPrismContext::create(std::int64_t id) {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called create on a null context");
    return {};
  }
  return _wrap(prism_registry_create(ctx, id_from_godot(id)));
}

Ref<GodotPrismBackend> GodotPrismContext::create_best() {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called create_best on a null context");
    return {};
  }
  return _wrap(prism_registry_create_best(ctx));
}

Ref<GodotPrismBackend> GodotPrismContext::acquire(std::int64_t id) {
  if (ctx == nullptr) {
    UtilityFunctions::push_error("Prism: called acquire on a null context");
    return {};
  }
  return _wrap(prism_registry_acquire(ctx, id_from_godot(id)));
}

Ref<GodotPrismBackend> GodotPrismContext::acquire_best() {
  if (ctx == nullptr) {
    UtilityFunctions::push_error(
        "Prism: called acquire_best on a null context");
    return {};
  }
  return _wrap(prism_registry_acquire_best(ctx));
}

TypedArray<Dictionary> GodotPrismContext::get_available_backends() const {
  TypedArray<Dictionary> result;
  auto const count = get_backends_count();
  for (int i = 0; i < count; ++i) {
    auto const bid = id_at(i);
    Dictionary d;
    d["id"] = bid;
    d["name"] = name_of(bid);
    d["priority"] = priority_of(bid);
    result.push_back(d);
  }
  return result;
}

void GodotPrismContext::_bind_methods() {
  // Backend ID constants
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "INVALID", id_to_godot(PRISM_BACKEND_INVALID));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "SAPI",
                                 id_to_godot(PRISM_BACKEND_SAPI));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "AV_SPEECH",
                                 id_to_godot(PRISM_BACKEND_AV_SPEECH));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "VOICE_OVER",
                                 id_to_godot(PRISM_BACKEND_VOICE_OVER));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "SPEECH_DISPATCHER",
                                 id_to_godot(PRISM_BACKEND_SPEECH_DISPATCHER));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "NVDA",
                                 id_to_godot(PRISM_BACKEND_NVDA));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "JAWS",
                                 id_to_godot(PRISM_BACKEND_JAWS));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "ONE_CORE",
                                 id_to_godot(PRISM_BACKEND_ONE_CORE));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ORCA",
                                 id_to_godot(PRISM_BACKEND_ORCA));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "ANDROID_TTS",
                                 id_to_godot(PRISM_BACKEND_ANDROID_TTS));
  ClassDB::bind_integer_constant(
      get_class_static(), "PrismBackendId", "ANDROID_SCREEN_READER",
      id_to_godot(PRISM_BACKEND_ANDROID_SCREEN_READER));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "WEB_SPEECH",
                                 id_to_godot(PRISM_BACKEND_WEB_SPEECH));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "UIA",
                                 id_to_godot(PRISM_BACKEND_UIA));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ZDSR",
                                 id_to_godot(PRISM_BACKEND_ZDSR));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "ZOOM_TEXT",
                                 id_to_godot(PRISM_BACKEND_ZOOM_TEXT));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "BOY_PC_READER",
                                 id_to_godot(PRISM_BACKEND_BOY_PC_READER));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "PC_TALKER",
                                 id_to_godot(PRISM_BACKEND_PC_TALKER));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "SENSE_READER",
                                 id_to_godot(PRISM_BACKEND_SENSE_READER));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "SYSTEM_ACCESS",
                                 id_to_godot(PRISM_BACKEND_SYSTEM_ACCESS));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "WINDOW_EYES",
                                 id_to_godot(PRISM_BACKEND_WINDOW_EYES));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "SPIEL",
                                 id_to_godot(PRISM_BACKEND_SPIEL));
  ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId",
                                 "GDK_SPEECH",
                                 id_to_godot(PRISM_BACKEND_GDK_SPEECH));
  BIND_ENUM_CONSTANT(PRISM_LOG_LEVEL_TRACE);
  BIND_ENUM_CONSTANT(PRISM_LOG_LEVEL_DEBUG);
  BIND_ENUM_CONSTANT(PRISM_LOG_LEVEL_INFO);
  BIND_ENUM_CONSTANT(PRISM_LOG_LEVEL_WARN);
  BIND_ENUM_CONSTANT(PRISM_LOG_LEVEL_ERROR);
  BIND_ENUM_CONSTANT(PRISM_LOG_LEVEL_NONE);
  // Signals
  ADD_SIGNAL(MethodInfo("backend_availability_changed",
                        PropertyInfo(Variant::INT, "id"),
                        PropertyInfo(Variant::STRING, "name"),
                        PropertyInfo(Variant::BOOL, "available")));
  ADD_SIGNAL(MethodInfo("availability_baseline_established"));
  ADD_SIGNAL(MethodInfo("log_message_received",
                        PropertyInfo(Variant::INT, "level"),
                        PropertyInfo(Variant::STRING, "source"),
                        PropertyInfo(Variant::STRING, "message")));
  // Methods
  ClassDB::bind_method(D_METHOD("is_backend_available", "id"),
                       &GodotPrismContext::is_backend_available);
  ClassDB::bind_method(
      D_METHOD("load_backend_library", "path", "priority_override"),
      &GodotPrismContext::load_backend_library, DEFVAL(-1));
  ClassDB::bind_method(D_METHOD("start_availability_monitoring",
                                "poll_interval_ms", "debounce_samples",
                                "backoff_max_ms", "auto_power_manage"),
                       &GodotPrismContext::start_availability_monitoring,
                       DEFVAL(0), DEFVAL(0), DEFVAL(0), DEFVAL(true));
  ClassDB::bind_method(D_METHOD("stop_availability_monitoring"),
                       &GodotPrismContext::stop_availability_monitoring);
  ClassDB::bind_method(D_METHOD("pause_availability_monitoring"),
                       &GodotPrismContext::pause_availability_monitoring);
  ClassDB::bind_method(D_METHOD("resume_availability_monitoring"),
                       &GodotPrismContext::resume_availability_monitoring);
  ClassDB::bind_method(D_METHOD("is_availability_monitoring"),
                       &GodotPrismContext::is_availability_monitoring);
  ClassDB::bind_static_method(
      get_class_static(), D_METHOD("is_auto_power_management_supported"),
      &GodotPrismContext::is_auto_power_management_supported);
  ClassDB::bind_method(D_METHOD("set_log_signal_enabled", "enabled"),
                       &GodotPrismContext::set_log_signal_enabled);
  ClassDB::bind_method(D_METHOD("is_log_signal_enabled"),
                       &GodotPrismContext::is_log_signal_enabled);
  ClassDB::bind_static_method(get_class_static(),
                              D_METHOD("set_log_level", "level"),
                              &GodotPrismContext::set_log_level);
  ClassDB::bind_static_method(
      get_class_static(), D_METHOD("write_log", "level", "source", "message"),
      &GodotPrismContext::write_log);
  ClassDB::bind_static_method(get_class_static(), D_METHOD("flush_log"),
                              &GodotPrismContext::flush_log);
  ClassDB::bind_static_method(get_class_static(),
                              D_METHOD("get_error_string", "error"),
                              &GodotPrismContext::get_error_string);
  ClassDB::bind_static_method(get_class_static(), D_METHOD("get_version"),
                              &GodotPrismContext::get_version);
  ClassDB::bind_static_method(get_class_static(),
                              D_METHOD("get_version_string"),
                              &GodotPrismContext::get_version_string);
  ClassDB::bind_method(D_METHOD("get_backends_count"),
                       &GodotPrismContext::get_backends_count);
  ClassDB::bind_method(D_METHOD("id_at", "index"), &GodotPrismContext::id_at);
  ClassDB::bind_method(D_METHOD("id_of", "name"), &GodotPrismContext::id_of);
  ClassDB::bind_method(D_METHOD("name_of", "id"), &GodotPrismContext::name_of);
  ClassDB::bind_method(D_METHOD("priority_of", "id"),
                       &GodotPrismContext::priority_of);
  ClassDB::bind_method(D_METHOD("has", "id"), &GodotPrismContext::has);
  ClassDB::bind_method(D_METHOD("get_backend", "id"),
                       &GodotPrismContext::get_backend);
  ClassDB::bind_method(D_METHOD("create", "id"), &GodotPrismContext::create);
  ClassDB::bind_method(D_METHOD("create_best"),
                       &GodotPrismContext::create_best);
  ClassDB::bind_method(D_METHOD("acquire", "id"), &GodotPrismContext::acquire);
  ClassDB::bind_method(D_METHOD("acquire_best"),
                       &GodotPrismContext::acquire_best);
  ClassDB::bind_method(D_METHOD("get_available_backends"),
                       &GodotPrismContext::get_available_backends);
  ADD_PROPERTY(PropertyInfo(Variant::INT, "backends_count"), "",
               "get_backends_count");
}
