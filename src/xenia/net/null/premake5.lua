project_root = "../../../.."
include(project_root.."/tools/build")

group("src")
project("xenia-net-null")
  uuid("03f823db-6d9f-4137-9ec4-5cfdc427fcf6")
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
