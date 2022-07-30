/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once
#include "xenia/hid/microphone/microphone_driver.h"
#include "SDL.h"

namespace xe {
namespace hid {
namespace microphone {
namespace sdl {

class SDLMicrophoneDriver final : public MicrophoneDriver {
 public:
  explicit SDLMicrophoneDriver();
  ~SDLMicrophoneDriver() override;

  X_STATUS Setup() override;

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           void* out_caps) override;
  X_RESULT GetState(uint32_t user_index, uint32_t* out_state) override;
  // X_RESULT SetState(uint32_t user_index, void* vibration) override;
  X_RESULT GetData(uint32_t user_index, void* out_keystroke) override;

 protected:
  SDL_AudioDeviceID sdl_device_id_ = -1;
  bool sdl_initialized_ = false;
  uint8_t sdl_device_channels_ = 0;

  uint32_t microphone_state_ = 0;
};

}  // namespace sdl
}  // namespace microphone
}  // namespace hid
}  // namespace xe