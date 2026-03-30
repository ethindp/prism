#pragma once
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/string.hpp"
#include <prism.h>
#include <cstdint>
#include <cstddef>
#include "godot_cpp/variant/builtin_types.hpp"
#include <limits>
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/callable.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/typed_array.hpp"

using namespace godot;

class GodotPrismBackend : public RefCounted {
	GDCLASS(GodotPrismBackend, RefCounted)
private:
	PrismBackend* backend;
protected:
	static void _bind_methods();
public:
	GodotPrismBackend(PrismBackend* raw);
	~GodotPrismBackend() override;
BitField<std::uint64_t> get_features() const;
String get_name() const;
PrismError speak(String text, bool interrupt);
PrismError speak_to_memory(String text, Callable function);
PrismError braille(String text);
PrismError output(String text, bool interrupt);
PrismError stop();
PrismError pause();
PrismError resume();
bool is_speaking() const;
void set_volume(float v);
float get_volume() const;
void set_pitch(float v);
float get_pitch() const;
void set_rate(float v);
float get_rate() const;
PrismError refresh_voices();
int voices_count() const;
Ref<String> get_voice_name(int index) const;
Ref<String> get_voice_language(int index) const;
void set_voice(int index);
int get_voice() const;
int get_channels() const;
int get_sample_rate() const;
int get_bit_depth() const;
