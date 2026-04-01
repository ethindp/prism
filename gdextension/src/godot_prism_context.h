#pragma once

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "godot_prism_backend.h"

#include <cstddef>
#include <cstdint>
#include <prism.h>

using namespace godot;

class GodotPrismContext : public Object {
  GDCLASS(GodotPrismContext, Object)

private:
  PrismContext *ctx = nullptr;

  static Ref<GodotPrismBackend> _wrap(PrismBackend *raw);

protected:
  static void _bind_methods();

public:
  GodotPrismContext();
  ~GodotPrismContext() override;
  int get_backends_count() const;
  int id_at(int index) const;
  int id_of(const String &name) const;
  String name_of(int id) const;
  int priority_of(int id) const;
  bool has(int id) const;
  Ref<GodotPrismBackend> get_backend(int id);
  Ref<GodotPrismBackend> create(int id);
  Ref<GodotPrismBackend> create_best();
  Ref<GodotPrismBackend> acquire(int id);
  Ref<GodotPrismBackend> acquire_best();
  TypedArray<Dictionary> get_available_backends() const;
};
