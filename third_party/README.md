# third_party

- `OpenCL-Headers/` — Khronos OpenCL C headers (vendored).
- `opencl-lib/` — MinGW import library for Windows `OpenCL.dll`.

Regenerate import lib (64-bit MinGW):

```powershell
cd G:\dev\repos\PFC\third_party\opencl-lib
gendef C:\Windows\System32\OpenCL.dll
dlltool -l libOpenCL.a -d OpenCL.def -k
```
