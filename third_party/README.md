# third_party

**Nothing is vendored in-tree.** OpenCL headers and the Windows import library are
fetched/generated at build time:

```bash
make deps      # → deps/OpenCL-Headers + deps/opencl-lib (Windows)
make build     # uses deps/
```

See the root `Makefile` (`OPENCL_HEADERS_REF`, `DEPS_DIR`).

| Path (generated) | Source |
|------------------|--------|
| `deps/OpenCL-Headers/` | [KhronosGroup/OpenCL-Headers](https://github.com/KhronosGroup/OpenCL-Headers) (pinned tag) |
| `deps/opencl-lib/libOpenCL.a` | `gendef` + `dlltool` against `OpenCL.dll` (Windows only) |

On Linux/macOS the apps link with system `-lOpenCL` (install `ocl-icd` / vendor ICD).
