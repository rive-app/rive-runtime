![Build Status](https://github.com/rive-app/rive-runtime/actions/workflows/tests.yml/badge.svg)
![Discord badge](https://img.shields.io/discord/532365473602600965)
![Twitter handle](https://img.shields.io/twitter/follow/rive_app.svg?style=social&label=Follow)


# rive-runtime

![Rive hero image](https://cdn.rive.app/rive_logo_dark_bg.png)

Rive's C++ runtime — the lowest-level Rive runtime. Loads `.riv` files,
advances state machines and animations, and draws via the abstract
`Renderer` interface. The built-in GPU renderer (`RiveRenderer`) has
`RenderContextImpl` backends for Metal, Vulkan, D3D11, D3D12, and OpenGL/WebGL.

Rive's Apple, Android, Flutter, Unity, Unreal, and web runtimes all wrap this
library.

Features:

- Loading artboards and their contents from `.riv` files.
- Querying state machines from artboards.
- Mutating the artboard hierarchy (the same mechanism used by state machines) and
  efficiently solving those changes via `Artboard::advance`.
- A state-of-the-art vector renderer for Metal, Vulkan, D3D12, D3D11, and
  OpenGL/WebGL.
- An abstract `Renderer` interface for hooking up an external vector renderer.

## Prerequisites

- A C++17 toolchain:
  - **macOS**: clang from Xcode Command Line Tools (`xcode-select --install`).
  - **Linux**: clang from your distro (e.g. `apt install clang`).
  - **Windows**: Visual Studio 2022 with the **C++ Clang Compiler for Windows**
    and **MSBuild support for LLVM (clang-cl) toolset** individual components.
- **git** — the build script clones a pinned `premake5` on first run.
  On Windows, install **Git for Windows** and during setup pick
  *"Use Git and optional Unix tools from the Command Prompt"* so that
  `sh.exe` ends up on PATH.
- Platform SDK for the renderer you're targeting:
  - macOS / iOS: Xcode.
  - Windows: Windows SDK (for D3D).
  - Linux: a Vulkan or OpenGL development environment.
  - Vulkan: the Vulkan SDK.

## Build

The build is driven by [premake5](https://premake.github.io/) wrapped by a
helper script:

- `build/build_rive.sh` — macOS / Linux (bash) / Windows (MinGW).
- `build/build_rive.ps1` — a PowerShell convenience wrapper for Windows; it
  shells out to the bash script, so a bash environment (Git for Windows /
  MinGW, see Prerequisites) must still be on PATH.

The helper self-installs the pinned premake version on first run and dispatches
to the right build system for your platform (gmake2 on macOS/Linux, MSBuild on
Windows). It must be run from a directory that contains a `premake5.lua` —
typically `tests/`, which builds the core library, the GPU renderer, and the
`player` sample app.

### macOS / Linux

```bash
git clone https://github.com/rive-app/rive-runtime.git
cd rive-runtime/tests
../build/build_rive.sh release
```

### Windows

```powershell
git clone https://github.com/rive-app/rive-runtime.git
cd rive-runtime\tests
..\build\build_rive.ps1 release
```

### Common variants

*(On Windows, substitute `build_rive.ps1` for `build_rive.sh`.)*

- `build_rive.sh` *(no args)* — debug build for the host.
- `build_rive.sh release clean` — clean then build release.
- `build_rive.sh ninja release` — use Ninja instead of make/MSBuild.
- `build_rive.sh ios release` — cross-compile for iOS.
- `build_rive.sh android release` — cross-compile for Android (defaults to arm64).
- `build_rive.sh ninja release wasm` — cross-compile for WebAssembly.
- `build_rive.sh --toolset=msc release` *(Windows)* — build with MSVC's cl.exe
  instead of clang-cl. The build supports both toolchains; the lua warning
  suppressions cover MSVC too.

See the comment header at the top of
[`build/build_rive.sh`](https://github.com/rive-app/rive-runtime/blob/main/build/build_rive.sh)
for the complete flag reference.

### Build outputs

Artifacts land in `out/<config>/` relative to the directory you built
from (typically `tests/`). The config directory encodes any OS/arch flags
you passed:

- `out/release`, `out/debug` — host build.
- `out/ios_release`, `out/android_arm64_release`, `out/wasm_release` —
  cross-compile builds.

| Artifact | Description |
| --- | --- |
| `librive.a` / `rive.lib` | Core runtime library. |
| `librive_pls_renderer.a` / `rive_pls_renderer.lib` | GPU renderer. |
| `player` / `player.exe` | Sample app — loads and renders a `.riv` file. |
| `out/<config>/goldens`, `out/<config>/gms`, `out/<config>/bench` | Test harness binaries (regression, golden-image, benchmarking). |
| `librive_decoders.a`, `librive_harfbuzz.a`, … | Supporting libraries. |

## Testing

The runtime's primary form of testing is **golden testing** — rendering
known scenes and diffing the output against checked-in reference images
via the `goldens` and `gms` test harness binaries (built into
`out/<config>/goldens` and `out/<config>/gms`).
See [`tests/`](https://github.com/rive-app/rive-runtime/tree/main/tests)
for how to run and rebaseline goldens.

Unit tests are secondary and use the
[Catch2](https://github.com/catchorg/Catch2) framework. From the repo root:

```bash
cd tests/unit_tests
./test.sh
```

Unit tests live in `tests/unit_tests/runtime/` (core runtime) and
`tests/unit_tests/renderer/` (renderer). To add a test, create an
`xxx_test.cpp` file in the appropriate directory — the harness picks it up
automatically.

## Code formatting

rive-runtime uses clang-format.

- **macOS**: `brew install clang-format`.
- **Windows**: already installed if you have the prerequisites — VS 2022's
  C++ Clang Compiler component ships `clang-format.exe` alongside
  `clang-cl.exe` at
  `C:\Program Files\Microsoft Visual Studio\2022\<edition>\VC\Tools\Llvm\x64\bin\`.
  You can also install LLVM standalone from
  [llvm.org](https://releases.llvm.org/) or via `winget install LLVM.LLVM`.
- **Linux**: install via your distro's package manager (e.g.
  `apt install clang-format`).

## Memory checks (macOS only)

To run the tests under macOS's built-in `leaks` tool:

```bash
cd tests/unit_tests
./test.sh memory
```

This wraps the test binary with `leaks --atExit`, which ships with macOS —
no install required. The `memory` flag is ignored on Linux and Windows.

## Disassembly explorer (macOS / Linux)

To inspect generated assembly per cpp file, install the
[Disassembly Explorer](https://marketplace.visualstudio.com/items?itemName=dseight.disasexpl)
VSCode extension. A `disassemble` task is provided in `.vscode/tasks.json`.
The underlying `gen assembly` task invokes `clang++` directly, so it needs
`clang++` on PATH — works out of the box on macOS (Xcode CLI tools) and most
Linux distros, but not on a default Windows + VS install (which provides
`clang-cl.exe`, not `clang++.exe`).

Reach the task from **Tasks: Run Task**, or bind a key in `keybindings.json`:

```json
[
    {
        "key": "cmd+d",
        "command": "workbench.action.tasks.runTask",
        "args": "disassemble"
    }
]
```
