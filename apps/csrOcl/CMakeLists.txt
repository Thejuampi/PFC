cmake_minimum_required(VERSION 3.16)
project(csrOcl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PFC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include("${PFC_ROOT}/cmake/EnsureOpenCLDeps.cmake")

add_executable(csrOcl src/main.cpp)

target_include_directories(csrOcl PRIVATE "${OPENCL_HEADERS_DIR}")
if(WIN32)
    target_link_directories(csrOcl PRIVATE "${OPENCL_LIB_DIR}")
endif()
target_link_libraries(csrOcl PRIVATE OpenCL)

add_custom_command(TARGET csrOcl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:csrOcl>/kernels"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/kernels/csr.cl"
        "$<TARGET_FILE_DIR:csrOcl>/kernels/csr.cl"
)

install(TARGETS csrOcl RUNTIME DESTINATION bin)
install(FILES kernels/csr.cl DESTINATION bin/kernels)
