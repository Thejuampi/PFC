#ifndef CLSPARSE_PCG_INIT_H
#define CLSPARSE_PCG_INIT_H

#include <stdio.h>
#include <iostream>
#include <vector>

#ifndef OMPI_MPI_H
#include <mpi/mpi.h>
#endif

#include "lduMatrix.H"

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#include"clSPARSE.h"


namespace clSparseUtils {

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
clsparseControl clSparseControl;
cl::Context g_context;

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
        return -3;
    }


    // Create clsparseControl object
    clSparseControl = clsparseCreateControl(queue(), &status); // supongo que el operador() debe estar sobrecargado
    if (status != CL_SUCCESS)
    {
        std::cout << "Problem with creating clSPARSE control object"
                  <<" error [" << status << "]" << std::endl;
        return -4;
    }


}

template <typename ValueType>
inline void importMatrix(const Foam::lduMatrix &foamMat, clsparseCsrMatrix *mat) {

    //TODO (juan) ver si esto es necesario cada ves, o si se puede "reutilizar" el espacio
    clsparseInitCsrMatrix(&A);

    // Matrix size
    int n = foamMat.diag().size();
    int nnz = foamMat.diag().size() + foamMat.lower().size() + foamMat.upper().size();

    // CSR values
    int *row_offset = NULL;
    int *col = NULL;
    ValueType *val = NULL;

    // reservar lugar:
    //TODO (juan) : ver si se puede hacer directamente en la memoria de la GPU
    row_offset = (std::nothrow) new int[n+1];// (n+1, &row_offset);
    col = (std::nothrow) new int[nnz];
    val = (std::nothrow) new ValueType[nnz];

    //Importar matriz aca
    row_offset[0] = 0;
    std::memset(row_offset+1, 1, n);

    Foam::UList<int>::const_iterator it_low = foamMat.lduAddr().lowerAddr().begin();
    Foam::UList<int>::const_iterator it_up  = foamMat.lduAddr().upperAddr().begin();

    for (int i=0; i<foamMat.lower().size(); ++i) {
      row_offset[*(it_low++) + 1]++;
      row_offset[*(it_up++)  + 1]++;
    }

    //CSR
    //TODO (juan) ver como se puede mejorar esto
    int sum = 0;
    for (int i=0; i<n; ++i) {
      int temp = row_offset[i];
      row_offset[i] = sum;
      sum += temp;
    }
    row_offset[n] = sum;

    Foam::UList<double>::const_iterator it_val = foamMat.lower().begin();
    it_low = foamMat.lduAddr().lowerAddr().begin();
    it_up  = foamMat.lduAddr().upperAddr().begin();

    // fill col and val arrays for lower part
    for (int i=0; i<foamMat.lower().size(); ++i) {
      // row index for lower = upper + 1
      int r_lower = *it_up + 1;
      int dest_lower = row_offset[r_lower];

      col[dest_lower] = *it_low;
      val[dest_lower] = *it_val;
      ++row_offset[r_lower];

      ++it_low;
      ++it_up;
      ++it_val;
    }

    // fill diagonal part
    it_val = foamMat.diag().begin();
    for (int i=0; i<foamMat.diag().size(); ++i) {
      int dest_diag = row_offset[i+1];
      val[dest_diag] = *it_val;
      col[dest_diag] = i;
      ++row_offset[i+1];
      ++it_val;
    }
    it_low = foam_mat.lduAddr().lowerAddr().begin();
    it_up  = foam_mat.lduAddr().upperAddr().begin();
    it_val = foam_mat.upper().begin();

    // fill upper part
    for (int i=0; i<foam_mat.upper().size(); ++i) {
      // row index for upper part = lower + 1
      int r_upper = *it_low + 1;
      int dest_upper = row_offset[r_upper];

      col[dest_upper] = *it_up;
      val[dest_upper] = *it_val;
      ++row_offset[r_upper];

      ++it_low;
      ++it_up;
      ++it_val;
    }

    mat->num_nonzeros = nnz;
    mat->num_cols = n;
    mat->num_rows = n;

    // REVISAR: probablemente no funcione bien con el "ValueType"
    mat->values = ::clCreateBuffer(clSparseUtils::g_context, CL_MEM_READ_ONLY, mat->num_nonzeros * sizeof( ValueType ), NULL, &cl_status );
    mat->colIndices = ::clCreateBuffer( context(), CL_MEM_READ_ONLY, mat->num_nonzeros * sizeof( cl_int ), NULL, &cl_status );
    mat->rowOffsets = ::clCreateBuffer( context(), CL_MEM_READ_ONLY, ( mat->num_rows + 1 ) * sizeof( cl_int ), NULL, &cl_status );
    mat->rowBlocks = ::clCreateBuffer( context(), CL_MEM_READ_ONLY, mat->rowBlockSize * sizeof( cl_ulong ), NULL, &cl_status );


    //OpenCL 2.0: usa clSVMAlloc <- para delegar la alocación de memoria en la GPU

//    clMemRAII( const cl_command_queue cl_queue, void* cl_malloc,
//                   const size_t cl_size = 0, const cl_svm_mem_flags cl_flags = CL_MEM_READ_WRITE):
//            clMem( nullptr ), clOwner(false)
//        {
//            clQueue = cl_queue;
//            clMem = static_cast< pType* >( cl_malloc );
//
//            if(cl_size > 0)
//            {
//                cl_context ctx = NULL;
//
//                ::clGetCommandQueueInfo(clQueue, CL_QUEUE_CONTEXT, sizeof( cl_context ), &ctx, NULL);
//                cl_int status = 0;
//
//                clMem = static_cast< pType* > (clSVMAlloc(ctx, cl_flags, cl_size * sizeof(pType), 0));
//                clOwner = true;
//            }
//
//            ::clRetainCommandQueue( clQueue );
//        }


    clMemRAII< cl_float > rCsrValues(   clSparseControl->queue( ), mat->values );
    clMemRAII< cl_int > rCsrColIndices( clSparseControl->queue( ), mat->colIndices );
    clMemRAII< cl_int > rCsrRowOffsets( clSparseControl->queue( ), mat->rowOffsets );

//    pType* clMapMem( cl_bool clBlocking, const cl_map_flags clFlags, const size_t clOff, const size_t clSize, cl_int *clStatus = nullptr)
//        {
//            // Right now, we don't support returning an event to wait on
//            clBlocking = CL_TRUE;
//
//            cl_int _clStatus = ::clEnqueueSVMMap( clQueue, clBlocking, clFlags,
//                                                  clMem, clSize * sizeof( pType ), 0, NULL, NULL );
//            if (clStatus != nullptr)
//            {
//                *clStatus = _clStatus;
//            }
//
//            return clMem;
//        }

     cl_float* fCsrValues = rCsrValues.clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,       mat->valOffset,     mat->num_nonzeros );
     cl_int* iCsrColIndices = rCsrColIndices.clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, mat->colIndOffset,  mat->num_nonzeros );
     cl_int* iCsrRowOffsets = rCsrRowOffsets.clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, mat->rowOffOffset,  mat->num_rows + 1 );

     //Esto de puede mejorar al copiar directamente al espacio de memoria de la GPU.
     // Por ahora lo dejo así porque necesito probar que funcione correctamente.
     // TODO:(juan) refactorizar esto. Hacer la asignación directamente al copiar los valores desde openFOAM
     std::memcpy(fCsrValues, val, nnz);
     std::memcpy(iCsrColIndices, col, nnz);
     std::memcpy(iCsrRowOffsets, row_offset, n);

}


}



#endif // CLSPARSE_PCG_INIT_H
