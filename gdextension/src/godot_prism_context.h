#pragma once
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/string.hpp"
#include <prism.h>
#include <cstdint>
#include <cstddef>
#include "godot_cpp/variant/builtin_types.hpp"
#include "godot_prism_backend.h"
#include <limits>
#include "godot_cpp/classes/ref.hpp"

using namespace godot;

class GodotPrismContext : public RefCounted {
	GDCLASS(GodotPrismContext, RefCounted)
private:
	PrismContext* ctx = nullptr;
protected:
	static void _bind_methods();
public:
	GodotPrismContext();
	~GodotPrismContext() override;
	std::size_t get_backends_count();
	PrismBackendId id_at(std::size_t index);
	PrismBackendId id_of(String name);
	String name_of(PrismBackendId id);
	int priority_of(PrismBackendId id);
	bool has(PrismBackendId id);
	Ref<GodotPrismBackend> get(PrismBackendId id);
	Ref<GodotPrismBackend> create(PrismBackendId id);
	Ref<GodotPrismBackend> create_best();
	Ref<GodotPrismBackend> acquire(PrismBackendId id);
	Ref<GodotPrismBackend> acquire_best();
};
