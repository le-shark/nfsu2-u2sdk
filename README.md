# u2sdk

Shared code for my scripts for Need for Speed Underground 2.

## Use

```cmake
include(path/to/u2sdk/cmake/u2flags.cmake)
add_subdirectory(path/to/u2sdk u2sdk)
u2_add_script(myscript NFSU2MyScript main.cpp)
```

## Build and test

Needs Visual Studio 2022 (or its Build Tools) with the C++ x86 toolset.

```powershell
.\build.ps1 -Test
```

## Licence

MIT (see `LICENSE`). `third_party/` keeps its own licences: MinHook (BSD 2-Clause) and stb_image_write (public
domain or MIT).
