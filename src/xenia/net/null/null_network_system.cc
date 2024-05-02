/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/net/null/null_network_system.h"

namespace xe {
namespace net {
namespace null {
NullNetworkSystem::NullNetworkSystem() {}

NullNetworkSystem::~NullNetworkSystem() {}

std::string NullNetworkSystem::name() const { return "null"; }
}  // namespace null
}  // namespace net
}  // namespace xe
