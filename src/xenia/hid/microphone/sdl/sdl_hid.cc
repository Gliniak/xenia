/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/microphone/sdl/sdl_hid.h"

#include "xenia/hid/microphone/sdl/sdl_microphone_driver.h"

namespace xe {
namespace hid {
namespace microphone {
namespace sdl {

std::unique_ptr<MicrophoneDriver> Create() {
  return std::make_unique<SDLMicrophoneDriver>();
}

}  // namespace microphone
}  // namespace sdl
}  // namespace hid
}  // namespace xe
