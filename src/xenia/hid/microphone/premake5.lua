project_root = "../../../.."
include(project_root.."/tools/build")

group("src")
project("xenia-hid-microphone")
  uuid("84b15cca-79fd-41ad-b386-4fedaf878e96")
  kind("StaticLib")
  language("C++")
  links({
    "xenia-base",
  })
  defines({
  })
  local_platform_files()