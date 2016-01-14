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

#include "clSPARSE.h"


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
clsparseControl g_clSparseControl;
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
    g_clSparseControl = clsparseCreateControl(queue(), &status); // supongo que el operador() debe estar sobrecargado
    if (status != CL_SUCCESS)
    {
        std::cout << "Problem with creating clSPARSE control object"
                  <<" error [" << status << "]" << std::endl;
        return -4;
    }


}

template <typename ValueType>
inline void importarMatrizDP(const Foam::lduMatrix &ref_foamMatrix, clsparseCsrMatrix *p_clSparseMatrix) {

    //TODO (juan) ver si esto es necesario cada ves, o si se puede "reutilizar" el espacio
    clsparseInitCsrMatrix(&A);

    // Matrix size
    int n = ref_foamMatrix.diag().size();
    int nnz = ref_foamMatrix.diag().size() + ref_foamMatrix.lower().size() + ref_foamMatrix.upper().size();

    // CSR values
    int *row_offsets = NULL;
    int *column_indices = NULL;
    ValueType *matrix_values = NULL;

    // reservar lugar:
    //TODO (juan) : ver si se puede hacer directamente en la memoria de la GPU
    row_offsets = (std::nothrow) new int[n+1];// (n+1, &row_offset);
    column_indices = (std::nothrow) new int[nnz];
    matrix_values = (std::nothrow) new ValueType[nnz];

    //Importar matriz aca
    row_offsets[0] = 0;
    std::memset(row_offsets+1, 1, n);

    Foam::UList<int>::const_iterator it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
    Foam::UList<int>::const_iterator it_up  = ref_foamMatrix.lduAddr().upperAddr().begin();

    for (int i=0; i<ref_foamMatrix.lower().size(); ++i) {
      row_offsets[*(it_low++) + 1]++;
      row_offsets[*(it_up++)  + 1]++;
    }

    //CSR
    //TODO (juan) ver como se puede mejorar esto
    int sum = 0;
    for (int i=0; i<n; ++i) {
      int temp = row_offsets[i];
      row_offsets[i] = sum;
      sum += temp;
    }
    row_offsets[n] = sum;

    Foam::UList<double>::const_iterator it_val = ref_foamMatrix.lower().begin();
    it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
    it_up  = ref_foamMatrix.lduAddr().upperAddr().begin();

    // fill col and val arrays for lower part
    for (int i=0; i<ref_foamMatrix.lower().size(); ++i) {
      // row index for lower = upper + 1
      int r_lower = *it_up + 1;
      int dest_lower = row_offsets[r_lower];

      column_indices[dest_lower] = *it_low;
      matrix_values[dest_lower] = *it_val;
      ++row_offsets[r_lower];

      ++it_low;
      ++it_up;
      ++it_val;
    }

    // fill diagonal part
    it_val = ref_foamMatrix.diag().begin();
    for (int i=0; i<ref_foamMatrix.diag().size(); ++i) {
      int dest_diag = row_offsets[i+1];
      matrix_values[dest_diag] = *it_val;
      column_indices[dest_diag] = i;
      ++row_offsets[i+1];
      ++it_val;
    }
    it_low = foam_mat.lduAddr().lowerAddr().begin();
    it_up  = foam_mat.lduAddr().upperAddr().begin();
    it_val = foam_mat.upper().begin();

    // fill upper part
    for (int i=0; i<foam_mat.upper().size(); ++i) {
      // row index for upper part = lower + 1
      int r_upper = *it_low + 1;
      int dest_upper = row_offsets[r_upper];

      column_indices[dest_upper] = *it_up;
      matrix_values[dest_upper] = *it_val;
      ++row_offsets[r_upper];

      ++it_low;
      ++it_up;
      ++it_val;
    }

    p_clSparseMatrix->num_nonzeros = nnz;
    p_clSparseMatrix->num_cols = n;
    p_clSparseMatrix->num_rows = n;

    // REVISAR: probablemente no funcione bien con el "ValueType"
    p_clSparseMatrix->values = ::clCreateBuffer(g_context(), CL_MEM_READ_ONLY, p_clSparseMatrix->num_nonzeros * sizeof( ValueType ), NULL, &cl_status );
    p_clSparseMatrix->colIndices = ::clCreateBuffer( g_context(), CL_MEM_READ_ONLY, p_clSparseMatrix->num_nonzeros * sizeof( cl_int ), NULL, &cl_status );
    p_clSparseMatrix->rowOffsets = ::clCreateBuffer( g_context(), CL_MEM_READ_ONLY, ( p_clSparseMatrix->num_rows + 1 ) * sizeof( cl_int ), NULL, &cl_status );
    p_clSparseMatrix->rowBlocks = ::clCreateBuffer( g_context(), CL_MEM_READ_ONLY, p_clSparseMatrix->rowBlockSize * sizeof( cl_ulong ), NULL, &cl_status );

    //OpenCL 2.0: usa clSVMAlloc <- para delegar la alocación de memoria en la GPU
    //FIXME!: ver como modificar cl_float/cl_double segun el TypeName
    clMemRAII< cl_double > rCsrValues(   g_clSparseControl->queue( ), p_clSparseMatrix->values );
    clMemRAII< cl_int > rCsrColIndices( g_clSparseControl->queue( ), p_clSparseMatrix->colIndices );
    clMemRAII< cl_int > rCsrRowOffsets( g_clSparseControl->queue( ), p_clSparseMatrix->rowOffsets );


    //FIXME! (juan) : ver como hacer cuando TypeName es float o es double
    //FIXME!: ver como modificar cl_float/cl_double segun el TypeName
    cl_double* fCsrValues = rCsrValues.clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,       p_clSparseMatrix->valOffset,     p_clSparseMatrix->num_nonzeros );
    cl_int* iCsrColIndices = rCsrColIndices.clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, p_clSparseMatrix->colIndOffset,  p_clSparseMatrix->num_nonzeros );
    cl_int* iCsrRowOffsets = rCsrRowOffsets.clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, p_clSparseMatrix->rowOffOffset,  p_clSparseMatrix->num_rows + 1 );

     //Esto de puede mejorar al copiar directamente al espacio de memoria de la GPU.
     // Por ahora lo dejo así porque necesito probar que funcione correctamente.
     // TODO:(juan) refactorizar esto. Hacer la asignación directamente al copiar los valores desde openFOAM
    std::memcpy(fCsrValues, matrix_values, nnz);
    std::memcpy(iCsrColIndices, column_indices, nnz);
    std::memcpy(iCsrRowOffsets, row_offsets, n);

    // This function allocates memory for rowBlocks structure. If not called
    // the structure will not be calculated and clSPARSE will run the vectorized
    // version of SpMV instead of adaptive;
    clsparseCsrMetaSize( p_clSparseMatrix, g_clSparseControl);
    A.rowBlocks = ::clCreateBuffer( context(), CL_MEM_READ_WRITE,
            A.rowBlockSize * sizeof( cl_ulong ), NULL, &cl_status );
    clsparseCsrMetaCompute( &A, control );

//    // Allocate memory for vector of unknowns;
//    g_x.num_values = g_A.num_cols;
//    g_x.values = clCreateBuffer(g_context(), CL_MEM_READ_ONLY, g_x.num_values * sizeof(TypeName), NULL, &cl_status);
//    TypeName zero(0.0);
//    TypeName one(1.0);
//    cl_status = clEnqueueFillBuffer(g_queue(), (cl_mem)g_x.values, &zero, sizeof(float),
//                                        0, g_x.num_values * sizeof(TypeName), 0, nullptr, nullptr);

}

/**
 *  Genera un vector de clSparse a partir de un vector de openFOAM
 *  Cuidado: No verifica puntero nulo
 */
template <typename ValueType>
inline void importarVectorOpenFoam(const Foam::scalarField &foamVector, cldenseVector *vector){
    size_t n = (size_t)foamVector.size();
    vector->values =        clCreateBuffer(g_context(), CL_MEM_READ_ONLY,n,NULL, &cl_status);
    /**
     * TODO (juan): Ver que es mas eficiente. Usar el mapeo de memoria de OpenCL 2.0 o copiar los datos directamente
     *
     */
    clMemRAII< cl_double >   rValues ( g_clSparseControl->queue( ), vector->values);
    cl_double*               fValues = rValues.clMapMem(CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0,  n);
    std::copy(foamVector.begin(), foamVector.end(), fValues);
}


}



#endif // CLSPARSE_PCG_INIT_H
