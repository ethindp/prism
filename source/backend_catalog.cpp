// SPDX-License-Identifier: MPL-2.0

#include "backend_catalog.h"
#include "logging.h"
#include <algorithm>
#include <ranges>

BackendCatalog &BackendCatalog::instance() {
  static BackendCatalog catalog;
  return catalog;
}

void BackendCatalog::add(Registration registration) {
logger.debug("Adding backend {} (name = {}, priority = {}, factory = {})", registration.id, registration.name, registration.priority, registration.factory
  std::lock_guard lock(mutex);
  registrations.push_back(std::move(registration));
}

std::vector<Registration> BackendCatalog::snapshot() const {
logger.trace("Snapshotting current catalog");
  std::lock_guard lock(mutex);
  return registrations;
}
