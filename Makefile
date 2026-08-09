# PFC — build / test / deps (OpenCL apps)
#
#   make            # deps + build both apps
#   make deps       # download OpenCL headers (+ Win import lib)
#   make build      # laplaceOcl + csrOcl
#   make test       # correctness smoke
#   make clean      # remove build artifacts
#   make distclean  # clean + remove downloaded deps
#   make help

ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
DEPS_DIR  ?= $(ROOT)/deps
BUILD_DIR ?= $(ROOT)/build

# Khronos OpenCL-Headers (pin a release tag for reproducibility)
OPENCL_HEADERS_REF ?= v2024.10.24
HDR_BASE  := https://raw.githubusercontent.com/KhronosGroup/OpenCL-Headers/$(OPENCL_HEADERS_REF)
HDR_DIR   := $(DEPS_DIR)/OpenCL-Headers
HDR_CL    := $(HDR_DIR)/CL
HDR_STAMP := $(HDR_DIR)/.fetched-$(OPENCL_HEADERS_REF)

CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -Wall -Wextra -I"$(HDR_DIR)"
LDFLAGS   ?=

# --- platform / portable shell helpers ---
ifeq ($(OS),Windows_NT)
  EXE         := .exe
  OPENCL_DLL  ?= C:/Windows/System32/OpenCL.dll
  OCL_LIBDIR  := $(DEPS_DIR)/opencl-lib
  OCL_LINK    := -L"$(OCL_LIBDIR)" -lOpenCL
  DEPS_LINK   := $(OCL_LIBDIR)/libOpenCL.a
  CURL        ?= curl.exe
  # cmd.exe-friendly helpers (paths may use /; cmd accepts them on modern Windows)
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

.PHONY: all deps headers opencl-lib build laplaceOcl csrOcl test test-laplace test-csr \
        clean distclean help run-laplace run-csr test-vram

all: build

help:
	@echo PFC Makefile targets:
	@echo   make / make all     deps + build
	@echo   make deps           download OpenCL headers (+ Win import lib)
	@echo   make build          laplaceOcl + csrOcl
	@echo   make laplaceOcl     build laplaceOcl only
	@echo   make csrOcl         build csrOcl only
	@echo   make test           run correctness smokes
	@echo   make test-laplace   2D/3D field + residual checks
	@echo   make test-csr       structured CSR smoke
	@echo   make test-vram      optional ~50 percent VRAM residual check
	@echo   make run-laplace    quick 100x100 run
	@echo   make run-csr        quick 64x64 CSR run
	@echo   make clean          remove build/
	@echo   make distclean      clean + remove deps/
	@echo Vars: DEPS_DIR=$(DEPS_DIR)  OPENCL_HEADERS_REF=$(OPENCL_HEADERS_REF)

# ---------------------------------------------------------------------------
# deps
# ---------------------------------------------------------------------------

deps: headers $(DEPS_LINK)

headers: $(HDR_STAMP)

$(HDR_STAMP):
	@$(call MKDIR,$(HDR_CL))
	@echo Fetching OpenCL-Headers @ $(OPENCL_HEADERS_REF) ...
	$(CURL) -fsSL -o "$(HDR_CL)/cl.h"          "$(HDR_BASE)/CL/cl.h"
	$(CURL) -fsSL -o "$(HDR_CL)/cl_platform.h" "$(HDR_BASE)/CL/cl_platform.h"
	$(CURL) -fsSL -o "$(HDR_CL)/cl_version.h"  "$(HDR_BASE)/CL/cl_version.h"
	$(CURL) -fsSL -o "$(HDR_DIR)/LICENSE"      "$(HDR_BASE)/LICENSE"
	@echo fetched $(OPENCL_HEADERS_REF)> "$(HDR_STAMP)"
	@echo OK headers - $(HDR_DIR)

ifeq ($(OS),Windows_NT)
opencl-lib: $(OCL_LIBDIR)/libOpenCL.a

$(OCL_LIBDIR)/libOpenCL.a: $(OPENCL_DLL)
	@$(call MKDIR,$(OCL_LIBDIR))
	@echo Generating MinGW import lib from $(OPENCL_DLL) ...
	cd /d "$(subst /,\,$(OCL_LIBDIR))" && gendef "$(OPENCL_DLL)"
	cd /d "$(subst /,\,$(OCL_LIBDIR))" && dlltool -l libOpenCL.a -d OpenCL.def -k -m i386:x86-64
	@echo OK $(OCL_LIBDIR)/libOpenCL.a
else
opencl-lib:
	@echo Using system -lOpenCL
endif

# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

build: laplaceOcl csrOcl

laplaceOcl: $(LAPLACE_BIN)
csrOcl: $(CSR_BIN)

$(LAPLACE_BIN): $(LAPLACE_SRC) $(LAPLACE_CL) $(HDR_STAMP) $(DEPS_LINK)
	@$(call MKDIR,$(LAPLACE_OUT)/kernels)
	$(CXX) $(CXXFLAGS) -o "$(LAPLACE_BIN)" "$(LAPLACE_SRC)" $(LDFLAGS) $(OCL_LINK)
	@$(call CP,$(LAPLACE_CL),$(LAPLACE_OUT)/kernels/laplace.cl)
	@echo OK $(LAPLACE_BIN)

$(CSR_BIN): $(CSR_SRC) $(CSR_CL) $(HDR_STAMP) $(DEPS_LINK)
	@$(call MKDIR,$(CSR_OUT)/kernels)
	$(CXX) $(CXXFLAGS) -o "$(CSR_BIN)" "$(CSR_SRC)" $(LDFLAGS) $(OCL_LINK)
	@$(call CP,$(CSR_CL),$(CSR_OUT)/kernels/csr.cl)
	@echo OK $(CSR_BIN)

# ---------------------------------------------------------------------------
# test / run
# ---------------------------------------------------------------------------

test: test-laplace test-csr
	@echo TEST PASS (all)

test-laplace: $(LAPLACE_BIN)
	@echo === laplaceOcl 256x256 CPU check ===
	"$(LAPLACE_BIN)" --nx 256 --ny 256 --steps 5 --tol 1e-8 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl" --no-csv --quiet
	@echo === laplaceOcl 32x32x32 3D ===
	"$(LAPLACE_BIN)" --nx 32 --ny 32 --nz 32 --steps 3 --tol 1e-8 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl" --no-csv --quiet
	@echo test-laplace OK

test-csr: $(CSR_BIN)
	@echo === csrOcl 64x64 ===
	"$(CSR_BIN)" --nx 64 --ny 64 --tol 1e-8 --kernels "$(CSR_OUT)/kernels/csr.cl"
	@echo === csrOcl 16x16x16 ===
	"$(CSR_BIN)" --nx 16 --ny 16 --nz 16 --tol 1e-8 --kernels "$(CSR_OUT)/kernels/csr.cl"
	@echo test-csr OK

test-vram: $(LAPLACE_BIN)
	"$(LAPLACE_BIN)" --mem-frac 0.5 --steps 1 --tol 1e-8 --max-iters 5000 --precond poly2 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl" --no-csv --quiet --no-cpu-check

run-laplace: $(LAPLACE_BIN)
	"$(LAPLACE_BIN)" --nx 100 --ny 100 --steps 10 --kernels "$(LAPLACE_OUT)/kernels/laplace.cl"

run-csr: $(CSR_BIN)
	"$(CSR_BIN)" --nx 64 --ny 64 --kernels "$(CSR_OUT)/kernels/csr.cl"

# ---------------------------------------------------------------------------
# clean
# ---------------------------------------------------------------------------

clean:
	-$(call RM_RF,$(BUILD_DIR))
	-$(call RM_RF,$(ROOT)/apps/laplaceOcl/build)
	-$(call RM_RF,$(ROOT)/apps/csrOcl/build)

distclean: clean
	-$(call RM_RF,$(DEPS_DIR))
