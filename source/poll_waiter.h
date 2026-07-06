// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <chrono>
#include <memory>
#include <optional>

class PollWaiter {
public:
  enum class Wake { Timer, Signal };

  virtual ~PollWaiter() = default;
  virtual Wake wait(std::optional<std::chrono::milliseconds> timeout,
                    std::chrono::milliseconds leeway) = 0;
  virtual void wake() = 0;
  [[nodiscard]] static std::unique_ptr<PollWaiter> create();
};
