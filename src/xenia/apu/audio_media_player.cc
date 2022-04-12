/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/apu/audio_driver.h"
#include "xenia/apu/audio_system.h"
#include "xenia/apu/audio_media_player.h"

extern "C" {
#if XE_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4101 4244 5033)
#endif
#include "third_party/FFmpeg/libavcodec/avcodec.h"
#include "third_party/FFmpeg/libavformat/avformat.h"
#if XE_COMPILER_MSVC
#pragma warning(pop)
#endif
}  // extern "C"


namespace xe {
namespace apu {

AudioMediaPlayer::AudioMediaPlayer(apu::AudioSystem* audio_system)
    : audio_system_(audio_system) {
  playback_state = PlaybackState::STOPPED;
  actual_song_index = 0;
  file_paths.clear();
};
AudioMediaPlayer::~AudioMediaPlayer(){};

void AudioMediaPlayer::Initialize() {
  playback_state = PlaybackState::STOPPED;
  actual_song_index = 0;
  file_paths.clear();
};

void AudioMediaPlayer::LoadPlaylist(){};
void AudioMediaPlayer::Play(){};
void AudioMediaPlayer::Pause(){};
void AudioMediaPlayer::Stop(){};


}  // namespace apu
}  // namespace xe