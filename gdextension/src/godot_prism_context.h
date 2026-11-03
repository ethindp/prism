#pragma once

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "godot_prism_backend.h"

#include <cstddef>
#include <cstdint>
#include <prism.h>
#include <string>
#include <vector>

using namespace godot;

class GodotPrismContext : public Object {
  GDCLASS(GodotPrismContext, Object)

private:
  struct BackendLibrary {
    std::string path;
    int priority_override = -1;
  };

  struct MonitoringOptions {
    std::uint32_t poll_interval_ms = 0;
    std::uint32_t debounce_samples = 0;
    std::uint32_t backoff_max_ms = 0;
    bool auto_power_manage = true;
  };

  PrismContext *ctx = nullptr;
  PrismContext *monitor_ctx = nullptr;
  PrismRegistry *registry = nullptr;
  MonitoringOptions monitoring_options;
  std::vector<BackendLibrary> backend_libraries;
  PrismLogHandler previous_log_handler{};
  bool log_signal_enabled = false;

  static Ref<GodotPrismBackend> _wrap(PrismBackend *raw);
  static void PRISM_CALL on_availability_changed(void *userdata,
                                                 PrismBackendId backend,
                                                 const char *name,
                                                 bool available);
  static void PRISM_CALL on_availability_baseline(void *userdata);
  static void PRISM_CALL on_log_message(void *userdata, PrismLogLevel level,
                                        const char *source,
                                        const char *message);
  PrismError open_monitor_context();

protected:
  static void _bind_methods();

public:
  GodotPrismContext();
  ~GodotPrismContext() override;
  std::int64_t get_backends_count() const;
  std::int64_t id_at(std::int64_t index) const;
  std::int64_t id_of(const String &name) const;
  String name_of(std::int64_t id) const;
  std::int64_t priority_of(std::int64_t id) const;
  bool has(std::int64_t id) const;
  Ref<GodotPrismBackend> get_backend(std::int64_t id);
  Ref<GodotPrismBackend> create(std::int64_t id);
  Ref<GodotPrismBackend> create_best();
  Ref<GodotPrismBackend> acquire(std::int64_t id);
  Ref<GodotPrismBackend> acquire_best();
  TypedArray<Dictionary> get_available_backends() const;
  bool is_backend_available(std::int64_t id);
  PrismError load_backend_library(const String &path,
                                  std::int64_t priority_override = -1);
  PrismError start_availability_monitoring(std::int64_t poll_interval_ms = 0,
                                           std::int64_t debounce_samples = 0,
                                           std::int64_t backoff_max_ms = 0,
                                           bool auto_power_manage = true);
  void stop_availability_monitoring();
  void pause_availability_monitoring();
  void resume_availability_monitoring();
  bool is_availability_monitoring() const;
  static bool is_auto_power_management_supported();
  void set_log_signal_enabled(bool enabled);
  bool is_log_signal_enabled() const;
  static PrismLogLevel set_log_level(PrismLogLevel level);
  static PrismError write_log(PrismLogLevel level, const String &source,
                              const String &message);
  static void flush_log();
  static String get_error_string(PrismError error);
  static std::int64_t get_version();
  static String get_version_string();
};

VARIANT_ENUM_CAST(PrismLogLevel);
