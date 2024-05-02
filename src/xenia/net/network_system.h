/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_NET_NETWORK_SYSTEM_H_
#define XENIA_NET_NETWORK_SYSTEM_H_

#include "xenia/memory.h"
#include "xenia/xbox.h"

namespace xe {
namespace net {

class NetworkSystem {
 public:
  virtual ~NetworkSystem(){};

  virtual std::string name() const = 0;

 protected:
  NetworkSystem();

 private:
};

}  // namespace net
}  // namespace xe

#endif  // XENIA_NET_NETWORK_SYSTEM_H_
