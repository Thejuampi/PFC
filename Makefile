# PFC — OpenCL apps (deps resolve automatically on first build)
#
#   make          build both apps (fetches OpenCL headers if needed)
#   make test     build if needed + run smokes
#   make help
#
# You do not need to run "make deps" by hand.

ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
DEPS_DIR  ?= $(ROOT)/deps
BUILD_DIR ?= $(ROOT)/build

OPENCL_HEADERS_REF ?= v2024.10.24
HDR_BASE  := https://raw.githubusercontent.com/KhronosGroup/OpenCL-Headers/$(OPENCL_HEADERS_REF)
HDR_DIR   := $(DEPS_DIR)/OpenCL-Headers
HDR_CL    := $(HDR_DIR)/CL
HDR_STAMP := $(HDR_DIR)/.fetched-$(OPENCL_HEADERS_REF)

CXX       ?= g++
# OpenMP: fair multi-thread CPU baseline in csrOcl (--force-cpu-check)
CXXFLAGS  ?= -std=c++17 -O2 -Wall -Wextra -fopenmp -I"$(HDR_DIR)"
LDFLAGS   ?= -fopenmp

ifeq ($(OS),Windows_NT)
  EXE         := .exe
  OPENCL_DLL  ?= C:/Windows/System32/OpenCL.dll
  OCL_LIBDIR  := $(DEPS_DIR)/opencl-lib
  OCL_LINK    := -L"$(OCL_LIBDIR)" -lOpenCL
  DEPS_LINK   := $(OCL_LIBDIR)/libOpenCL.a
  CURL        ?= curl.exe
  MKDIR       = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
  RM_RF       = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
  CP          = copy /Y "$(subst /,\,$(1))" "$(subst /,\,$(2))" >NUL
else
  EXE         :=
  OCL_LINK    := -lOpenCL
  DEPS_LINK   :=
  CURL        ?= curl
  MKDIR       = mkdir -p $(1)
  RM_RF       = rm -rf $(1)
  CP          = cp -f $(1) $(2)
endif

LAPLACE_SRC := $(ROOT)/apps/laplaceOcl/src/main.cpp
LAPLACE_CL  := $(ROOT)/apps/laplaceOcl/kernels/laplace.cl
LAPLACE_OUT := $(BUILD_DIR)/laplaceOcl
LAPLACE_BIN := $(LAPLACE_OUT)/laplaceOcl$(EXE)

CSR_SRC     := $(ROOT)/apps/csrOcl/src/main.cpp
CSR_CL      := $(ROOT)/apps/csrOcl/kernels/csr.cl
CSR_OUT     := $(BUILD_DIR)/csrOcl
CSR_BIN     := $(CSR_OUT)/csrOcl$(EXE)

.PHONY: all build test check laplaceOcl csrOcl test-laplace test-csr test-vram \
        run run-laplace run-csr clean distclean help deps headers opencl-lib \
        bench-speedup

# One-command happy path: build (auto-deps) then smoke tests
all: test

help:
	@echo PFC — nothing to configure for OpenCL third-party bits.
	@echo.
	@echo   make           build + test  (recommended first command)
	@echo   make build     compile laplaceOcl + csrOcl only
	@echo   make test      same as make  (build if needed, then smokes)
	@echo   make run       quick demo run of both apps
	@echo   make clean     remove build/
	@echo   make distclean remove build/ and cached deps/
	@echo   make bench-speedup   GPU vs OpenMP CPU on windTunnelCar pressure (idle machine!)
	@echo.
	@echo Headers and the Windows OpenCL import lib are fetched on first build.
	@echo See docs/CPU_GPU_SPEEDUP.md for the scientific comparison protocol.

build: laplaceOcl csrOcl

check test: test-laplace test-csr
	@echo.
	@echo All tests passed.

laplaceOcl: $(LAPLACE_BIN)
csrOcl: $(CSR_BIN)

# ---------------------------------------------------------------------------
# Auto deps (internal — triggered by binary rules, not a user step)
# ---------------------------------------------------------------------------

# Keep `make deps` as a no-op convenience alias for CI/scripts
deps: $(HDR_STAMP) $(DEPS_LINK)
	@echo deps ready under $(DEPS_DIR)

headers: $(HDR_STAMP)

$(HDR_STAMP):
	@$(call MKDIR,$(HDR_CL))
	@echo [deps] downloading OpenCL-Headers $(OPENCL_HEADERS_REF) ...
	@$(CURL) -fsSL -o "$(HDR_CL)/cl.h"          "$(HDR_BASE)/CL/cl.h"
	@$(CURL) -fsSL -o "$(HDR_CL)/cl_platform.h" "$(HDR_BASE)/CL/cl_platform.h"
	@$(CURL) -fsSL -o "$(HDR_CL)/cl_version.h"  "$(HDR_BASE)/CL/cl_version.h"
	@$(CURL) -fsSL -o "$(HDR_DIR)/LICENSE"      "$(HDR_BASE)/LICENSE"
	@echo fetched $(OPENCL_HEADERS_REF)> "$(HDR_STAMP)"

ifeq ($(OS),Windows_NT)
$(OCL_LIBDIR)/libOpenCL.a: $(OPENCL_DLL)
	@$(call MKDIR,$(OCL_LIBDIR))
	@echo [deps] generating MinGW import lib from OpenCL.dll ...
	@cd /d "$(subst /,\,$(OCL_LIBDIR))" && gendef "$(OPENCL_DLL)" >NUL
	@cd /d "$(subst /,\,$(OCL_LIBDIR))" && dlltool -l libOpenCL.a -d OpenCL.def -k -m i386:x86-64
else
# no file prerequisite on non-Windows
endif

# ---------------------------------------------------------------------------
# Compile
# ---------------------------------------------------------------------------

$(LAPLACE_BIN): $(LAPLACE_SRC) $(LAPLACE_CL) $(HDR_STAMP) $(DEPS_LINK)
	@$(call MKDIR,$(LAPLACE_OUT)/kernels)
	@echo [build] laplaceOcl
	@$(CXX) $(CXXFLAGS) -o "$(LAPLACE_BIN)" "$(LAPLACE_SRC)" $(LDFLAGS) $(OCL_LINK)
	@$(call CP,$(LAPLACE_CL),$(LAPLACE_OUT)/kernels/laplace.cl)

$(CSR_BIN): $(CSR_SRC) $(CSR_CL) $(HDR_STAMP) $(DEPS_LINK)
	@$(call MKDIR,$(CSR_OUT)/kernels)
	@echo [build] csrOcl
	@$(CXX) $(CXXFLAGS) -o "$(CSR_BIN)" "$(CSR_SRC)" $(LDFLAGS) $(OCL_LINK)
	@$(call CP,$(CSR_CL),$(CSR_OUT)/kernels/csr.cl)

# ---------------------------------------------------------------------------
# Test / run
# ---------------------------------------------------------------------------

test-laplace: $(LAPLACE_BIN)
	@echo [test] laplaceOcl 256x256
	@"$(LAPLACE_BIN)" --nx 256 --ny 256 --steps 5 --tol 1e-8 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl" --no-csv --quiet
	@echo [test] laplaceOcl 32x32x32
	@"$(LAPLACE_BIN)" --nx 32 --ny 32 --nz 32 --steps 3 --tol 1e-8 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl" --no-csv --quiet

test-csr: $(CSR_BIN)
	@echo [test] csrOcl 64x64
	@"$(CSR_BIN)" --nx 64 --ny 64 --tol 1e-8 --kernels "$(CSR_OUT)/kernels/csr.cl"
	@echo [test] csrOcl 16x16x16
	@"$(CSR_BIN)" --nx 16 --ny 16 --nz 16 --tol 1e-8 --kernels "$(CSR_OUT)/kernels/csr.cl"

test-vram: $(LAPLACE_BIN)
	@"$(LAPLACE_BIN)" --mem-frac 0.5 --steps 1 --tol 1e-8 --max-iters 5000 --precond poly2 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl" --no-csv --quiet --no-cpu-check

run: run-laplace run-csr

run-laplace: $(LAPLACE_BIN)
	@"$(LAPLACE_BIN)" --nx 100 --ny 100 --steps 10 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl"

run-csr: $(CSR_BIN)
	@"$(CSR_BIN)" --nx 64 --ny 64 --kernels "$(CSR_OUT)/kernels/csr.cl"

# Fair GPU vs multi-thread CPU on the car tunnel pressure system (same A,b).
# Requires: cases/windTunnelCar/matrix/of_p.{mtx,rhs}  (ofDumpCsr once).
# Run only on an IDLE machine (no AI training / heavy load).
bench-speedup: $(CSR_BIN)
	@echo === bench-speedup: windTunnelCar pressure CSR  GPU vs OpenMP CPU ===
	@echo Protocol: docs/CPU_GPU_SPEEDUP.md
	@"$(CSR_BIN)" --mtx "$(ROOT)/cases/windTunnelCar/matrix/of_p.mtx" \
		--rhs "$(ROOT)/cases/windTunnelCar/matrix/of_p.rhs" \
		--kernels "$(CSR_OUT)/kernels/csr.cl" \
		--force-cpu-check --tol 1e-8 --max-iters 5000

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------

clean:
	-$(call RM_RF,$(BUILD_DIR))
	-$(call RM_RF,$(ROOT)/apps/laplaceOcl/build)
	-$(call RM_RF,$(ROOT)/apps/csrOcl/build)

distclean: clean
	-$(call RM_RF,$(DEPS_DIR))
