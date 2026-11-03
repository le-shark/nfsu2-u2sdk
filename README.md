# u2sdk

Shared code for ASI scripts for Need for Speed Underground 2 (v1.2, `SPEED2.EXE`): logging, INI reading, memory
and hook helpers (MinHook), game state, input, FEng (the game's UI) helpers, a native-looking options menu, D3D9
helpers, screenshots and a crash logger.

Used by [NFSU2 Photo Mode](../nfsu2-photomode) as a git submodule.

## Use

```cmake
include(path/to/u2sdk/cmake/u2flags.cmake)   # when your project is the top-level one
add_subdirectory(path/to/u2sdk u2sdk)
u2_add_script(myscript NFSU2MyScript main.cpp)   # builds NFSU2MyScript.asi linked with u2sdk
```

## Build and test

Needs Visual Studio 2022 (or its Build Tools) with the C++ x86 toolset. NFSU2 is 32-bit.

```powershell
.\build.ps1 -Test
```

## Licence

MIT (see `LICENSE`). `third_party/` keeps its own licences: MinHook (BSD 2-Clause) and stb_image_write (public
domain or MIT).
