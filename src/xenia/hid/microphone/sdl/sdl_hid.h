/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_MICROPHONE_SDL_SDL_HID_H_
#define XENIA_HID_MICROPHONE_SDL_SDL_HID_H_

#include <memory>

#include "xenia/hid/microphone/microphone_system.h"

namespace xe {
namespace hid {
namespace microphone {
namespace sdl {


std::unique_ptr<MicrophoneDriver> Create();

}  // namespace controller
}  // namespace sdl
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_MICROPHONE_SDL_SDL_HID_H_
