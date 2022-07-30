project_root = "../../../../.."
include(project_root.."/tools/build")

group("src")
project("xenia-hid-microphone-sdl")
  uuid("89c75eac-8209-4b10-b5b8-d3be990b24f0")
  kind("StaticLib")
  language("C++")
  links({
    "xenia-base",    
    "xenia-helper-sdl",
    "SDL2",
  })
  defines({
  })
  local_platform_files()
  sdl2_include()