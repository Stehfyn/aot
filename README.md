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

# aot

**A love letter to the Win32 API in C89**

Adds an **Always On Top** toggle to the system menu of *any* window on Windows, 32-bit
and 64-bit applications alike.

## Usage

1. Run `AlwaysOnTop.exe`. An icon appears in the system tray.
2. Open any window's **system menu** (press `Alt`+`Space`, or right-click its title bar). A new **Always On Top** entry is there.
3. Click it to pin the window on top; the entry shows a checkmark while it's active. Click again to unpin.
4. **Right-click** the tray icon to quit.

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
