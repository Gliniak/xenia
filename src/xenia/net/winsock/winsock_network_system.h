/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_NET_WINSOCK_WINSOCK_NETWORK_SYSTEM_H_
#define XENIA_NET_WINSOCK_WINSOCK_NETWORK_SYSTEM_H_

#include "xenia/net/network_system.h"

namespace xe {
namespace net {
namespace winsock {

class WinsockNetworkSystem : public NetworkSystem {
 public:
  WinsockNetworkSystem();
  ~WinsockNetworkSystem() override;

  static bool IsAvailable() { return true; }

  std::string name() const override;
};

}  // namespace winsock
}  // namespace net
}  // namespace xe

#endif  // XENIA_NET_WINSOCK_WINSOCK_NETWORK_SYSTEM_H_
