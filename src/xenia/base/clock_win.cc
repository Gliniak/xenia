/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2019 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/clock.h"

#include "xenia/base/platform_win.h"

namespace xe {

uint64_t Clock::host_tick_frequency_platform() {
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart;
}

uint64_t Clock::host_tick_count_platform() {
  LARGE_INTEGER counter;
  uint64_t time = 0;
  if (QueryPerformanceCounter(&counter)) {
    time = counter.QuadPart;
  }
  return time;
}

uint64_t Clock::QueryHostSystemTime() {
  FILETIME t;
  GetSystemTimeAsFileTime(&t);
  return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
}

uint64_t Clock::QueryHostStfsTime() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    // STFS accuracy is 2 seconds.
    st.wSecond -= st.wSecond % 2;
    // Remove unnecessary part.
    st.wMilliseconds = 0;

    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    return (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

uint64_t Clock::QueryHostUptimeMillis() {
  return host_tick_count_platform() * 1000 / host_tick_frequency_platform();
}

}  // namespace xe
