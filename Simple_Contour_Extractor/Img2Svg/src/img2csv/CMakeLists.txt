include(FetchContent)

FetchContent_Declare(
  stb URL https://github.com/nothings/stb/archive/refs/heads/master.zip)

FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
  message(STATUS "'stb' library not found, downloading from GitHub....")
  FetchContent_MakeAvailable(stb)
endif()

add_executable(img2csv main.cpp)

target_include_directories(img2csv PRIVATE ${stb_SOURCE_DIR})
