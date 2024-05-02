/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/net/winsock/winsock_network_system.h"

namespace xe {
namespace net {
namespace winsock {
WinsockNetworkSystem::WinsockNetworkSystem() {}

WinsockNetworkSystem::~WinsockNetworkSystem() {}

std::string WinsockNetworkSystem::name() const { return "winsock"; }
}  // namespace winsock
}  // namespace net
}  // namespace xe
