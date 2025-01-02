<!--
  Title: AlwaysOnTop: A C89-ish love letter to the Win32 api
  Description: A demoscene-esque implementation of the ill-famed CBT-Hook, adding an HWND_TOPMOST menu item to an activated Windows window's sysmenu. Tp
  accomodate both 32-bit and 64-bit applications seamlessly and simultaneously, multiple release binaries targeting the two platforms are used. In total, the final .exe
  contains 8 binaries (4 .dlls, 4 .exe(s)). This data is embedded alongside the application manifest, resource info, and version info, clocking in total at a
  mightily lean 1000 lines of C89-ish C.
  Author: Stehfyn
  Links:
  - https://github.com/Stehfyn
  - https://youtu.be/AgTcB26K44Q?si=tdZqElSopUzALcTg
  -->

AlwaysOnTop hook utility written in C
  - Single file (aot.c) (~1000 sloc)
  - Supports both 32-bit and 64-bit applications
  - 9 binaries (Yes, that's right; `AlwaysOnTop.exe` has embedded within **4 dlls** and **4 executables**)

https://github.com/user-attachments/assets/d07e49d3-dd72-44e0-9bc6-bc5dba2501dc

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
