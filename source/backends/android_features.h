// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <bitset>
#include <cstdint>
#include <utility>

// Djinni flags are 32 bits wide, so the feature set crosses in two halves.
template <typename JavaBackend>
std::bitset<64> combined_features(JavaBackend &backend) {
  const auto low =
      static_cast<std::uint32_t>(std::to_underlying(backend.get_features()));
  const auto high = static_cast<std::uint32_t>(
      std::to_underlying(backend.get_features_high()));
  return std::bitset<64>{(static_cast<std::uint64_t>(high) << 32) | low};
}
