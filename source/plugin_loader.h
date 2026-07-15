// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "frozen_registry.h"
#include "prism.h"
#include <cstddef>

PrismError load_plugin(RegistryBuilder &builder, const char *path,
                       int priority_override, std::size_t *out_count);