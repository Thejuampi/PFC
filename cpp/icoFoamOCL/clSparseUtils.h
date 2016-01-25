#pragma once
#ifndef CLSPARSE_PCG_INIT_H
#define CLSPARSE_PCG_INIT_H

#include <stdio.h>
#include <iostream>
#include <vector>

//#ifndef OMPI_MPI_H
//#include <mpi/mpi.h>
//#endif

#include "lduMatrix.H"

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#define BUILD_CLVERSION 200

#include <clSPARSE.h>
#include "clSPARSE-2x.hpp"
//#include <clSPARSE-2x.hpp"

/*
 *
 *    Don't define variables in headers. Put declarations in header and definitions in one of the .c files.
 *    In config.h
 *
 *    extern const char *names[];
 *    In some .c file:
 *
 *    const char *names[] =
 *    {
 *       "brian", "stefan", "steve"
 *    };
 *
 */

/**
 * If you have more than just main.cpp, and include your test.h, then each .cpp file will have its own copy of testNum.
 * If you want them to share then you need all but one to mark it as extern.
 */

namespace clSparseUtils {

///**
// * @brief Variables de mpi
// */
//extern int ierr, my_id, num_procs;

/**
 * @brief Variables de OpenCL
 */
extern cl::Device g_device;
extern cl::Platform g_platform;
extern cl::CommandQueue g_queue;
extern cl_int cl_status;
extern std::vector<cl::Platform> g_platforms;
extern std::vector<cl::Device> g_devices;

/**
 * @brief Varibales de clSPARSE
 */
extern cldenseVector g_x;
extern cldenseVector g_b;
extern clsparseCsrMatrix g_A;
extern clsparseStatus status;
extern clsparseControl g_clSparseControl;
extern cl::Context g_context;

cl_int getDeviceId();
cl_int getPlatformId();
void init();

template <typename ValueType=double>
inline void importarMatrizDP(const Foam::lduMatrix &ref_foamMatrix, clsparseCsrMatrix *p_clSparseMatrix);
template <typename ValueType=double>
inline void importarVectorOpenFoam(const Foam::scalarField &foamVector, cldenseVector *vector);

}

#endif // CLSPARSE_PCG_INIT_H
