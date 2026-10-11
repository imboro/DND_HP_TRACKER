# Windows build

Everything for the Windows version lives in this folder, apart from the Mac
build (`build/`, `dist/`). The source code is shared: `gui_main.cpp` has a few
`#ifdef _WIN32` spots (fonts, display scaling, scroll speed, save paths).

| File | What it is |
|---|---|
| `build_windows.sh` | Builds the Windows app **on a Mac** and packages it |
| `mingw-toolchain.cmake` | Tells CMake to use the llvm-mingw Windows compiler |
| `README.txt` | Short instructions shipped inside the zip, for the people you send it to |

Generated (not in git):

| Folder | Contents |
|---|---|
| `deps/` | The llvm-mingw compiler (~125 MB) and SDL2 for Windows, downloaded once (versions and checksums pinned in the script) |
| `build/` | CMake build tree |
| `dist/` | `DnD HP Tracker/` (the `.exe`, `SDL2.dll`, `README.txt`) and `DnD-HP-Tracker-windows.zip` |

## Option A: build on the Mac

```
windows/build_windows.sh
```

The first run downloads the compiler and SDL2 into `windows/deps/` (a couple of
minutes); later runs only rebuild. Only CMake needs to be installed
(`brew install cmake`).

Send `windows/dist/DnD-HP-Tracker-windows.zip`. It runs on 64-bit Windows 10
and 11; nothing needs installing (the C++ runtime is built into the `.exe`).

## Option B: build on a Windows PC

1. Install **Visual Studio 2022 Community** with the *Desktop development with
   C++* workload (includes CMake), and **vcpkg** (`git clone
   https://github.com/microsoft/vcpkg` then `.\vcpkg\bootstrap-vcpkg.bat`).
2. `.\vcpkg\vcpkg install sdl2:x64-windows`
3. From the project folder, in a *Developer PowerShell*:

```
cmake -S . -B windows\build -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build windows\build --config Release
```

The app is `windows\build\Release\DnD HP Tracker.exe`; vcpkg copies
`SDL2.dll` next to it.

## Differences from the Mac version

- Fonts: Arial / Arial Bold / Arial Black from `C:\Windows\Fonts` (Helvetica
  isn't on Windows), so the look matches the Mac.
- Shortcuts are shown as Ctrl (Ctrl+Z, Ctrl+D...). Both versions accept either.
- Saves go to `%APPDATA%\HotelWorkshop\HPTracker\`.
- The app isn't code-signed, so Windows SmartScreen asks once: *More info* >
  *Run anyway*.
