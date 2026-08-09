# Auto-fetch OpenCL headers (and Win import lib) into ${PFC_ROOT}/deps.
# Included from app CMakeLists — no manual "make deps" step.

if(NOT DEFINED PFC_ROOT)
    message(FATAL_ERROR "PFC_ROOT must be set before including EnsureOpenCLDeps.cmake")
endif()

if(DEFINED ENV{PFC_DEPS_DIR})
    set(PFC_DEPS_DIR "$ENV{PFC_DEPS_DIR}")
else()
    set(PFC_DEPS_DIR "${PFC_ROOT}/deps")
endif()

set(OPENCL_HEADERS_REF "v2024.10.24" CACHE STRING "Khronos OpenCL-Headers git tag")
set(OPENCL_HEADERS_DIR "${PFC_DEPS_DIR}/OpenCL-Headers")
set(OPENCL_HDR_BASE
    "https://raw.githubusercontent.com/KhronosGroup/OpenCL-Headers/${OPENCL_HEADERS_REF}")

function(pfc_download_hdr relpath)
    set(_out "${OPENCL_HEADERS_DIR}/${relpath}")
    if(EXISTS "${_out}")
        return()
    endif()
    get_filename_component(_dir "${_out}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    message(STATUS "[deps] downloading ${relpath}")
    file(DOWNLOAD
        "${OPENCL_HDR_BASE}/${relpath}"
        "${_out}"
        STATUS _st
        TLS_VERIFY ON
    )
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _st 1 _msg)
        message(FATAL_ERROR "Failed to download OpenCL header ${relpath}: ${_msg}")
    endif()
endfunction()

if(NOT EXISTS "${OPENCL_HEADERS_DIR}/CL/cl.h")
    message(STATUS "[deps] OpenCL headers missing — fetching ${OPENCL_HEADERS_REF}")
    pfc_download_hdr("CL/cl.h")
    pfc_download_hdr("CL/cl_platform.h")
    pfc_download_hdr("CL/cl_version.h")
    pfc_download_hdr("LICENSE")
endif()

set(OPENCL_LIB_DIR "${PFC_DEPS_DIR}/opencl-lib")
if(WIN32)
    set(_implib "${OPENCL_LIB_DIR}/libOpenCL.a")
    if(NOT EXISTS "${_implib}")
        set(_dll "C:/Windows/System32/OpenCL.dll")
        if(NOT EXISTS "${_dll}")
            message(FATAL_ERROR
                "OpenCL.dll not found at ${_dll}. Install your GPU OpenCL driver.")
        endif()
        find_program(GENDEF_EXE gendef)
        find_program(DLLTOOL_EXE dlltool)
        if(NOT GENDEF_EXE OR NOT DLLTOOL_EXE)
            message(FATAL_ERROR
                "gendef/dlltool not found (need MinGW). Or run: make  (from repo root)")
        endif()
        file(MAKE_DIRECTORY "${OPENCL_LIB_DIR}")
        message(STATUS "[deps] generating ${_implib} from OpenCL.dll")
        execute_process(
            COMMAND "${GENDEF_EXE}" "${_dll}"
            WORKING_DIRECTORY "${OPENCL_LIB_DIR}"
            RESULT_VARIABLE _gd
        )
        if(NOT _gd EQUAL 0)
            message(FATAL_ERROR "gendef failed (${_gd})")
        endif()
        execute_process(
            COMMAND "${DLLTOOL_EXE}" -l libOpenCL.a -d OpenCL.def -k -m i386:x86-64
            WORKING_DIRECTORY "${OPENCL_LIB_DIR}"
            RESULT_VARIABLE _dt
        )
        if(NOT _dt EQUAL 0)
            message(FATAL_ERROR "dlltool failed (${_dt})")
        endif()
    endif()
endif()
