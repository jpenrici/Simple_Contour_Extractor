cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(
  Img2Svg
  VERSION 1.0.0
  DESCRIPTION "Simple bitmap image outline extractor and SVG converter."
  LANGUAGES CXX Fortran)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_Fortran_STANDARD 2018)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "Debug"
      CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(src/img2csv)

# add_subdirectory(src/csv2json)

# add_subdirectory(src/json2svg)
