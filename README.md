<!--
  Title: AlwaysOnTop: A C89-ish love letter to the Win32 API
  Description: A demoscene-esque implementation of the ill-famed CBT hook, adding an
  HWND_TOPMOST menu item to any activated window's system menu. To serve both 32-bit
  and 64-bit applications seamlessly and simultaneously, release binaries targeting
  both platforms are used. The final .exe embeds 8 binaries (4 .dlls, 4 .exes)
  alongside the application manifest, icon, and version info, clocking in at a
  mightily lean ~1000 lines of C89-ish C.
  Author: Stehfyn
  Links:
  - https://github.com/Stehfyn
  - https://youtu.be/AgTcB26K44Q?si=tdZqElSopUzALcTg
  -->

# AlwaysOnTop

**A C89-ish love letter to the Win32 API.**

Adds an **Always On Top** toggle to the system menu of *any* window on Windows, 32-bit
and 64-bit applications alike. The whole thing is one file of C, and ships as a single
self-contained `.exe` that unpacks and orchestrates everything it needs at runtime.

![C](https://img.shields.io/badge/C-C89--ish-blue)
![Platform](https://img.shields.io/badge/platform-Windows-0078D6)
![Arch](https://img.shields.io/badge/arch-x86%20%7C%20x64-informational)
![License](https://img.shields.io/badge/license-MIT-green)

https://github.com/user-attachments/assets/d07e49d3-dd72-44e0-9bc6-bc5dba2501dc

## Highlights

- **One source file** — the entire project lives in [`aot.c`](aot.c), ~1000 lines of C89-ish C.
- **One binary to ship** — `AlwaysOnTop.exe` carries **9 binaries in total**: itself, plus **4 DLLs and 4 EXEs** embedded as resources and unpacked on demand.
- **Both architectures at once** — a 32-bit and a 64-bit hook chain run side by side, so the toggle works in every app regardless of bitness.
- **No installer, no dependencies** — just run it. It lives in the system tray and cleans up after itself via a job object.

## Usage

1. Build it (see below), then run `out\bin\AlwaysOnTop.exe`. An icon appears in the system tray.
2. Open any window's **system menu** (press `Alt`+`Space`, or right-click its title bar). A new **Always On Top** entry is there.
3. Click it to pin the window on top; the entry shows a checkmark while it's active. Click again to unpin.
4. **Right-click** the tray icon to quit.

## How it works

`AlwaysOnTop.exe` is a 32-bit launcher that never touches disk with anything you have to
manage. At startup it shows a tray icon and, on a worker thread, unpacks and launches
both host executables, one per architecture. Each host in turn unpacks its payload and
installs a `WH_CBT` hook. When a window is activated, the injected hook DLL appends the
**Always On Top** item to that window's system menu; choosing it flips the window's
`WS_EX_TOPMOST` style. Two architectures are required because a hook DLL can only be
injected into processes of its own bitness.

Everything is nested as embedded resources:

```
AlwaysOnTop.exe                  x86 launcher · tray UI · job-object supervisor
├── aotx86-host.exe              32-bit host
│   ├── aot-host.dll
│   ├── aot-hook.dll             32-bit CBT hook payload (adds the menu item)
│   └── aot-hook.exe             installs the hook
└── aotx64-host.exe             64-bit host
    ├── aot-host.dll
    ├── aot-hook.dll             64-bit CBT hook payload
    └── aot-hook.exe             installs the hook
```

The same `aot.c` compiles into every one of these binaries; which role it plays is
selected at compile time by preprocessor defines (`_RELEASE`, `_HOST`, `_AOTHOOKDLL`,
`_AOTHOSTDLL`, and so on), and the resources are built with `rc` and linked in.

## Building

You need **Visual Studio** with the **C++ desktop toolset** (any recent edition,
including Build Tools). Then:

```bat
build.bat
```

`build.bat` locates your toolset automatically via `vswhere`, builds both architectures,
and produces the final launcher at:

```
out\bin\AlwaysOnTop.exe
```

Each compiler and resource-compiler command is echoed as it runs, so you can follow
exactly what the build does.

## Project layout

| File                 | Purpose                                             |
| -------------------- | --------------------------------------------------- |
| `aot.c`              | The entire program — every binary compiles from it. |
| `aot.manifest`       | Application manifest (embedded).                     |
| `aot.ico`            | Tray / application icon (embedded).                  |
| `build.bat`          | Builds all binaries and the final launcher.         |

## License

```
MIT License

Copyright (c) 2024 Stephen Foster

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
