#include "godot_prism_context.h"

#include "godot_cpp/variant/utility_functions.hpp"

#include <bit>
#include <cassert>
#include <limits>

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
  if (ctx != nullptr) {
    prism_shutdown(ctx);
    ctx = nullptr;
  }
}

std::int64_t GodotPrismContext::get_backends_count() const {
  if (ctx == nullptr) {
    UtilityFunctions::push_error(
        "Prism: called get_backends_count on a null context");
    return 0;
  }
  auto const n = prism_registry_count(ctx);
  if (n == std::numeric_limits<std::size_t>::max()) {
    return 0;
  }
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
    return String();
  }
  auto const bid = id_from_godot(id);
  if (bid == PRISM_BACKEND_INVALID) {
    return String();
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
  // Methods
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
