project_root = "../../.."
include(project_root.."/tools/build")

group("src")
project("xenia-net")
  uuid("6a499b01-208c-47be-a291-a9768a37df1c")
  kind("StaticLib")
  language("C++")
  links({
    "fmt",
    "xenia-base",
    "xenia-ui",
    "xxhash",
  })
  includedirs({
  })
  local_platform_files()
