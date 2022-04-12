/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APU_AUDIO_MEDIA_PLAYER_H_
#define XENIA_APU_AUDIO_MEDIA_PLAYER_H_

#include "xenia/apu/audio_system.h"

namespace xe {
namespace apu {

enum class PlaybackState { STOPPED = 0, PLAYING = 1, PAUSED = 2 };

class AudioMediaPlayer {
 public:
  AudioMediaPlayer(apu::AudioSystem* audio_system);
  ~AudioMediaPlayer();

  void Initialize();
  void LoadPlaylist();

  void Play();
  void Stop();
  void Pause();

 private:
  apu::AudioSystem* audio_system_ = nullptr;

  PlaybackState playback_state;
  uint32_t actual_song_index;

  std::vector<std::filesystem::path> file_paths;

  //bool decode_audio_media();

};

}  // namespace apu
}  // namespace xe

#endif


