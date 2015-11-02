#ifndef CLSPARSE_PCG_INIT_H
#define CLSPARSE_PCG_INIT_H

#include <iostream>
#include <vector>

#ifndef OMPI_MPI_H
#include <mpi/mpi.h>
#endif

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

namespace Tj {

/**
 * @brief Variables de mpi
 */
int ierr, my_id, num_procs;

/**
 * @brief Variables de OpenCL
 */
cl::Device g_device;
cl::Platform g_platform;
cl::CommandQueue g_queue;
cl_int cl_status;
std::vector<cl::Platform> g_platforms;
std::vector<cl::Device> g_devices;

/**
 * @brief Varibales de clSPARSE
 */
cldenseVector g_x;
cldenseVector g_b;
clsparseCsrMatrix g_A;
clsparseStatus status;

cl_int getDeviceId() {
    if(num_procs > my_id) {
        my_id = my_id % num_procs;
    }
    return my_id;
}

cl_int getPlatformId() {
    return 0; // Por el momento, solo funciona en un solo host
}

void init() {

    ierr = MPI_Comm_rank(MPI_COMM_WORLD, &my_id);
    ierr = MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    /**  Step 1. Setup OpenCL environment; **/

    // Init OpenCL environment;
    cl_status = CL_SUCCESS;

    // Get OpenCL platforms
    cl_status = cl::Platform::get(&g_platforms);

    if (cl_status != CL_SUCCESS)
    {
        std::cout << "Problem with getting OpenCL platforms"
                  << " [" << cl_status << "]" << std::endl;
        return -2;
    }

    int platform_id = getPlatformId();
    for (const auto& p : g_platforms)
    {
        std::cout << "Platform ID " << platform_id++ << " : "
                  << p.getInfo<CL_PLATFORM_NAME>() << std::endl;

    }

    // Platform
    platform_id = getPlatformId();
    g_platform = g_platforms[platform_id];

    // Get device from platform
    cl_status = g_platform.getDevices(CL_DEVICE_TYPE_GPU, &g_devices);

    if (cl_status != CL_SUCCESS)
    {
        std::cout << "Problem with getting devices from platform"
                  << " [" << platform_id << "] " << g_platform.getInfo<CL_PLATFORM_NAME>()
                  << " error: [" << cl_status << "]" << std::endl;
    }

    std::cout << std::endl
              << "Getting devices from platform " << platform_id << std::endl;
    cl_int device_id = getDeviceId();
//    for (const auto& device : devices)
//    {
//        std::cout << "Device ID " << device_id++ << " : "
//                  << device.getInfo<CL_DEVICE_NAME>() << std::endl;
//    }

    // Device;
    device_id = getDeviceId();
    g_device = g_devices[device_id];

    // Create OpenCL context;
    cl::Context context(g_device);

    // Create OpenCL queue;
    cl::CommandQueue queue(context, g_device);

    /** Step 2. Setup GPU buffers **/

    //we will allocate it after matrix will be loaded;
    clsparseInitVector(&g_x);
    clsparseInitVector(&g_b);
    clsparseInitCsrMatrix(&g_A);


    /** Step 3. Init clSPARSE library **/

    status = clsparseSetup();
    if (status != clsparseSuccess)
    {
        std::cout << "Problem with executing clsparseSetup()" << std::endl;
        return -3;
    }


    // Create clsparseControl object
    clsparseControl control = clsparseCreateControl(queue(), &status);
    if (status != CL_SUCCESS)
    {
        std::cout << "Problem with creating clSPARSE control object"
                  <<" error [" << status << "]" << std::endl;
        return -4;
    }



}


}



#endif // CLSPARSE_PCG_INIT_H
