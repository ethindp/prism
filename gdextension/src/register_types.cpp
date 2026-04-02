#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "godot_prism_backend.h"
#include "godot_prism_context.h"
#include <cassert>

using namespace godot;

static GodotPrismContext *prism_singleton = nullptr;

void initialize_prism_types(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  GDREGISTER_CLASS(GodotPrismBackend);
  GDREGISTER_CLASS(GodotPrismContext);
  prism_singleton = memnew(GodotPrismContext);
  assert(prism_singleton);
  Engine::get_singleton()->register_singleton("Prism", prism_singleton);
}

void uninitialize_prism_types(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  Engine::get_singleton()->unregister_singleton("Prism");
  if (prism_singleton != nullptr) {
    memdelete(prism_singleton);
    prism_singleton = nullptr;
  }
}

extern "C" {
GDExtensionBool GDE_EXPORT
prism_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                   GDExtensionClassLibraryPtr p_library,
                   GDExtensionInitialization *r_initialization) {
  GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library,
                                          r_initialization);
  init_obj.register_initializer(initialize_prism_types);
  init_obj.register_terminator(uninitialize_prism_types);
  init_obj.set_minimum_library_initialization_level(
      MODULE_INITIALIZATION_LEVEL_SCENE);
  return init_obj.init();
}

} // extern "C"
