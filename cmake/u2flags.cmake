# Compiler settings shared by every NFSU2 ASI build. Included by whichever project is at the top (the scripts
# workspace, a script repo built on its own, or u2sdk itself), before add_subdirectory(u2sdk).
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "NFSU2 is 32-bit: configure with an x86 toolchain (use build.ps1)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_compile_options(/W3 /utf-8 /permissive- /Zc:__cplusplus)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
