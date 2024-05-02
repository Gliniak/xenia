/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_NET_NULL_NULL_NETWORK_SYSTEM_H_
#define XENIA_NET_NULL_NULL_NETWORK_SYSTEM_H_

#include "xenia/net/network_system.h"

namespace xe {
namespace net {
namespace null {

class NullNetworkSystem : public NetworkSystem {
 public:
  NullNetworkSystem();
  ~NullNetworkSystem() override;

  static bool IsAvailable() { return true; }

  std::string name() const override;
};

}  // namespace null
}  // namespace net
}  // namespace xe

#endif  // XENIA_NET_NULL_NULL_NETWORK_SYSTEM_H_
