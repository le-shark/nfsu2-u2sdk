cmake_minimum_required(VERSION 3.20)
# u2sdk: shared code for the NFSU2 ASI scripts. Used with add_subdirectory() by a script's build, or built on its own
# (its unit tests).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(u2sdk LANGUAGES C CXX)
  include(cmake/u2flags.cmake)
  set(U2SDK_TOP ON)
endif()

# ---- third party ----------------------------------------------------------
set(MH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook)
add_library(minhook STATIC
  ${MH}/src/buffer.c ${MH}/src/hook.c ${MH}/src/trampoline.c ${MH}/src/hde/hde32.c)
target_include_directories(minhook PUBLIC ${MH}/include)

# ---- u2sdk ------------------------------------------------------------------
add_library(u2sdk STATIC
  src/log.cpp
  src/ini.cpp
  src/mem.cpp
  src/hook.cpp
  src/game.cpp
  src/input.cpp
  src/feng.cpp
  src/optmenu.cpp
  src/freeze.cpp
  src/d3d.cpp
  src/dev.cpp
  src/texture.cpp
  src/crash.cpp)
target_include_directories(u2sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(u2sdk PUBLIC minhook d3d9 xinput9_1_0 ole32 windowscodecs)

# ---- helper: build an ASI script ------------------------------------------
# u2_add_script(<target> <OutputName> sources...)
function(u2_add_script target outname)
  add_library(${target} SHARED ${ARGN})
  target_link_libraries(${target} PRIVATE u2sdk)
  set_target_properties(${target} PROPERTIES OUTPUT_NAME ${outname} SUFFIX ".asi")
endfunction()

# ---- host unit tests (pure code, no game needed) ---------------------------
# tests/test.h + tests/test_main.cpp are the harness the scripts' tests use too.
if(U2SDK_TOP)
  add_executable(u2sdk_tests tests/test_main.cpp tests/test_math.cpp)
  target_include_directories(u2sdk_tests PRIVATE include tests)
endif()
