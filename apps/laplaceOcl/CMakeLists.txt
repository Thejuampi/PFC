cmake_minimum_required(VERSION 3.16)
project(laplaceOcl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PFC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include("${PFC_ROOT}/cmake/EnsureOpenCLDeps.cmake")

add_executable(laplaceOcl src/main.cpp)

target_include_directories(laplaceOcl PRIVATE "${OPENCL_HEADERS_DIR}")
if(WIN32)
    target_link_directories(laplaceOcl PRIVATE "${OPENCL_LIB_DIR}")
endif()
target_link_libraries(laplaceOcl PRIVATE OpenCL)

add_custom_command(TARGET laplaceOcl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:laplaceOcl>/kernels"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/kernels/laplace.cl"
        "$<TARGET_FILE_DIR:laplaceOcl>/kernels/laplace.cl"
)

install(TARGETS laplaceOcl RUNTIME DESTINATION bin)
install(FILES kernels/laplace.cl DESTINATION bin/kernels)
