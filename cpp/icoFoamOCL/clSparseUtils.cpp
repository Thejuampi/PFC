#include "clSparseUtils.h"

namespace clSparseUtils{

cl_int getDeviceId() {
//    if(num_procs > my_id) {
//        my_id = my_id % num_procs;
//    }
//    return my_id;
    return 0;
}

cl_int getPlatformId() {
    return 0; // Por el momento, solo funciona en un solo host
}


void init() {

    //ierr = MPI_Comm_rank(MPI_COMM_WORLD, &my_id);
    //ierr = MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    /**  Step 1. Setup OpenCL environment; **/

    // Init OpenCL environment;
    cl_status = CL_SUCCESS;

    // Get OpenCL platforms
    cl_status = cl::Platform::get(&g_platforms);

    if (cl_status != CL_SUCCESS)
    {
        std::cout << "Problem with getting OpenCL platforms"
                  << " [" << cl_status << "]" << std::endl;
        //return -2;
        exit(-2);
    }

    int platform_id = getPlatformId();
    for (const cl::Platform& p : g_platforms)
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
    g_context = cl::Context(g_device);

    // Create OpenCL queue;
    cl::CommandQueue queue(g_context, g_device);

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
        exit(-3);
    }

    // Create clsparseControl object
    g_clSparseControl = clsparseCreateControl(queue(), &status); // supongo que el operador() debe estar sobrecargado
    if (status != CL_SUCCESS)
    {
        std::cout << "Problem with creating clSPARSE control object"
                  <<" error [" << status << "]" << std::endl;
        return exit(-4);
    }

}



} // namespace

