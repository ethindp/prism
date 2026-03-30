#include "godot_prism_context.h"

GodotPrismContext::GodotPrismContext() {
auto const cfg = prism_config_init();
ctx = prism_init(&cfg);
if (ctx == nullptr) {
push_error("Prism: Prism context creation failed!");
}
}

GodotPrismContext::~GodotPrismContext() {
if (ctx != nullptr) {
prism_shutdown(ctx);
}
}

std::size_t GodotPrismContext::get_backends_count() {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return std::numeric_limits<std::size_t>::max();
}
return prism_registry_count(ctx);
}

PrismBackendId GodotPrismBackend::id_at(std::size_t index) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return PRISM_BACKEND_INVALID;
}
return prism_registry_id_at(ctx, id);
}

PrismBackendId GodotPrismBackend::id_of(String name) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return PRISM_BACKEND_INVALID;
}
if (name.is_empty()) {
return PRISM_BACKEND_INVALID;
}
return prism_registry_id(ctx, name.utf8().get_data());
}

String GodotPrismContext::name_of(PrismBackendId id) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return String::utf8("Invalid");
}
if (id == PRISM_BACKEND_INVALID) {
return String::utf8("Invalid");
}
return String::utf8(prism_registry_name(ctx, id));
}

int GodotPrismContext::priority_of(PrismBackendId id) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return -1;
}
return prism_registry_priority(ctx, id);
}

bool GodotPrismContext::has(PrismBackendId id) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return false;
}
return prism_registry_exists(ctx, id);
}

Ref<GodotPrismBackend> GodotPrismContext::get(PrismBackendId id) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return {};
}
Ref<GodotPrismBackend> backend;
backend.instantiate(prism_registry_get(ctx, id));
return backend;
}

Ref<GodotPrismBackend> GodotPrismContext::create(PrismBackendId id) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return {};
}
Ref<GodotPrismBackend> backend;
backend.instantiate(prism_registry_create(ctx, id));
return backend;
}

Ref<GodotPrismBackend> GodotPrismContext::create_best() {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return {};
}
Ref<GodotPrismBackend> backend;
backend.instantiate(prism_registry_create_best(ctx));
return backend;
}

Ref<GodotPrismBackend> GodotPrismContext::acquire(PrismBackendId id) {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return {};
}
Ref<GodotPrismBackend> backend;
backend.instantiate(prism_registry_acquire(ctx, id));
return backend;
}

Ref<GodotPrismBackend> GodotPrismContext::acquire_best() {
if (ctx == nullptr) {
push_error("Prism: Critical: called a prism method on a null context");
return {};
}
Ref<GodotPrismBackend> backend;
backend.instantiate(prism_registry_acquire_best(ctx));
return backend;
}

void GodotPrismContext::_bind_methods() {
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "INVALID", PRISM_BACKEND_INVALID);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "SAPI", PRISM_BACKEND_SAPI);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "AV_SPEECH", PRISM_BACKEND_AV_SPEECH);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "VOICE_OVER", PRISM_BACKEND_VOICE_OVER);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "SPEECH_DISPATCHER", PRISM_BACKEND_SPEECH_DISPATCHER);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "NVDA", PRISM_BACKEND_NVDA);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "JAWS", PRISM_BACKEND_JAWS);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ONE_CORE", PRISM_BACKEND_ONE_CORE);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ORCA", PRISM_BACKEND_ORCA);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ANDROID_TTS", PRISM_BACKEND_ANDROID_TTS);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ANDROID_SCREEN_READER", PRISM_BACKEND_ANDROID_SCREEN_READER);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "WEB_SPEECH", PRISM_BACKEND_WEB_SPEECH);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "UIA", PRISM_BACKEND_UIA);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ZDSR", PRISM_BACKEND_ZDSR);
ClassDB::bind_integer_constant(get_class_static(), "PrismBackendId", "ZOOM_TEXT", PRISM_BACKEND_ZOOM_TEXT);
ClassDB::bind_method(D_METHOD("get_backends_count"), &GodotPrismContext::get_backends_count);
ADD_PROPERTY(PropertyInfo(Variant::INT, "backends_count"), "", "get_backends_count");
ClassDB::bind_method(D_METHOD("id_at", "int"), &GodotPrismContext::id_at);
ClassDB::bind_method(D_METHOD("id_of", "string"), &GodotPrismContext::id_of);
ClassDB::bind_method(D_METHOD("name_of", "int"), &GodotPrismContext::name_of);
ClassDB::bind_method(D_METHOD("priority_of", "int"), &GodotPrismContext::priority_of);
ClassDB::bind_method(D_METHOD("has", "int"), &GodotPrismContext::has);
ClassDB::bind_method(D_METHOD("get", "int"), &GodotPrismContext::get);
ClassDB::bind_method(D_METHOD("create", "int"), &GodotPrismContext::create);
ClassDB::bind_method(D_METHOD("create_best"), &GodotPrismContext::create_best);
ClassDB::bind_method(D_METHOD("acquire", "int"), &GodotPrismContext::acquire);
ClassDB::bind_method(D_METHOD("acquire_best"), &GodotPrismContext::acquire_best);
}
