// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <functional>
#include <memory>

class PowerNotifier {
public:
  virtual ~PowerNotifier() = default;
  [[nodiscard]] static std::unique_ptr<PowerNotifier>
  create(const std::function<void()> &on_suspend,
         const std::function<void()> &on_resume);
  [[nodiscard]] static bool supported() noexcept;
};
