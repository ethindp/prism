// SPDX-License-Identifier: MPL-2.0

#include "plugin_loader.h"

PrismError load_plugin([[maybe_unused]] RegistryBuilder &builder,
                       [[maybe_unused]] const char *path,
                       [[maybe_unused]] int priority_override,
                       std::size_t *out_count) {
  if (out_count != nullptr)
    *out_count = 0;
  return PRISM_ERROR_NOT_IMPLEMENTED;
}
