project_root = "../../../.."
include(project_root.."/tools/build")

group("src")
project("xenia-net-winsock")
  uuid("7722b897-fdb4-4410-af99-2903ac7acbeb")
  kind("StaticLib")
  language("C++")
  links({
    "xenia-base",
    "xenia-net",
    "xxhash",
  })
  includedirs({
  })
  local_platform_files()
