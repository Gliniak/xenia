/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/logging.h"

#include "xenia/hid/microphone/sdl/sdl_microphone_driver.h"
#include "xenia/helper/sdl/sdl_helper.h"

namespace xe {
namespace hid {
namespace microphone {
namespace sdl {
SDLMicrophoneDriver::SDLMicrophoneDriver() : MicrophoneDriver(){};

SDLMicrophoneDriver::~SDLMicrophoneDriver(){};

X_STATUS SDLMicrophoneDriver::Setup() {
  // for (int i = 0; i < SDL_GetNumAudioDevices((int)true); i++) {
  //  std::string name = SDL_GetAudioDeviceName(i, (int)true);
  //  XELOGE("{} - Audio capture device: {}", i, name);
  //}

  SDL_version ver = {};
  SDL_GetVersion(&ver);
  if ((ver.major < 2) || (ver.major == 2 && ver.minor == 0 && ver.patch < 8)) {
    XELOGW(
        "SDL library version {}.{}.{} is outdated. "
        "You may experience choppy audio.",
        ver.major, ver.minor, ver.patch);
  }

  if (!xe::helper::sdl::SDLHelper::Prepare()) {
    return false;
  }
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    return false;
  }


  SDL_AudioSpec desired_spec = {};
  desired_spec.freq = 48000;
  desired_spec.format = AUDIO_F32;
  desired_spec.channels = 1;
  desired_spec.samples = 1;
  desired_spec.callback = nullptr;
  desired_spec.userdata = this;

  SDL_AudioSpec obtained_spec;

  int capture_devices_amount = SDL_GetNumAudioDevices(SDL_TRUE);
  XELOGI("Connected Microphones:");
  for (int i = 0; i < capture_devices_amount; i++) {
    std::string name = SDL_GetAudioDeviceName(i, SDL_TRUE);
    XELOGE("{} - Audio capture device: {}", i, name);
  }

  for (int i = 0; i < capture_devices_amount; i++) {
    std::string device_name = SDL_GetAudioDeviceName(i, SDL_TRUE);
    sdl_device_id_ = SDL_OpenAudioDevice(
        device_name.c_str(), SDL_TRUE, &desired_spec,
                                         &obtained_spec, SDL_FALSE);
    if (sdl_device_id_ <= 0) {
      XELOGE("SDL_OpenAudioDevice() failed.");
      return false;
    }

    if (sdl_device_id_ <= 0) {
      XELOGE("Failed to get a compatible SDL Audio Device.");
      return false;
    }
    sdl_device_channels_ = obtained_spec.channels;
  }

  SDL_PauseAudioDevice(sdl_device_id_, 0);
  microphone_state_ = 0;
  return true;
}

X_RESULT SDLMicrophoneDriver::GetCapabilities(uint32_t user_index,
                                              uint32_t flags, void* out_caps) {
  return X_STATUS_SUCCESS;
}

X_RESULT SDLMicrophoneDriver::GetState(uint32_t user_index,
                                       uint32_t* out_state) {
  *out_state = microphone_state_;
  if (microphone_state_ == 3) {
    microphone_state_ = 5;
  }

  if (microphone_state_ < 3 && microphone_state_ > 0) {
    microphone_state_++;
  }
  if (microphone_state_ > 5) {
    microphone_state_ = 5;
  }
  if (!microphone_state_) {
    microphone_state_ = user_index + 1;
  }
  return X_STATUS_SUCCESS;
}

X_RESULT SDLMicrophoneDriver::GetData(uint32_t user_index, void* out_ptr) {
  return X_STATUS_SUCCESS;
}

}  // namespace sdl
}  // namespace microphone
}  // namespace hid
}  // namespace xe