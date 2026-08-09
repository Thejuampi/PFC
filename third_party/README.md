# third_party

- `OpenCL-Headers/` — **minimal** Khronos OpenCL C headers (`cl.h` + platform + version only). Not a full upstream clone.
- `opencl-lib/` — MinGW import library for Windows `OpenCL.dll`.

Refresh headers from upstream when needed:

```powershell
# copy CL/cl.h CL/cl_platform.h CL/cl_version.h + LICENSE from
# https://github.com/KhronosGroup/OpenCL-Headers
```

Regenerate import lib (64-bit MinGW):

```powershell
cd G:\dev\repos\PFC\third_party\opencl-lib
gendef C:\Windows\System32\OpenCL.dll
dlltool -l libOpenCL.a -d OpenCL.def -k
```
