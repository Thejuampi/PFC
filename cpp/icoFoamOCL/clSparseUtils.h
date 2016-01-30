#pragma once
#ifndef CLSPARSE_PCG_INIT_H
#define CLSPARSE_PCG_INIT_H

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#ifndef BUILD_CLVERSION
#define BUILD_CLVERSION 200
#endif

#ifndef WM_DP
#define WM_DP
#endif

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

//cl_int getDeviceId();
//cl_int getPlatformId();
//void init();

template<typename ValueType = double>
void importarMatrizDP(const Foam::lduMatrix &ref_foamMatrix, clsparseCsrMatrix *p_clSparseMatrix, cl_context context, cl_command_queue queue, clsparseControl control) {

	//TODO (juan) ver si esto es necesario cada ves, o si se puede "reutilizar" el espacio
	clsparseInitCsrMatrix(p_clSparseMatrix);

	// Matrix size
	int n = ref_foamMatrix.diag().size();
	int nnz = ref_foamMatrix.diag().size() + ref_foamMatrix.lower().size()
			+ ref_foamMatrix.upper().size();

	// CSR values
	int *row_offsets = NULL;
	int *column_indices = NULL;
	ValueType *matrix_values = NULL;

	// reservar lugar:
	//TODO (juan) : ver si se puede hacer directamente en la memoria de la GPU
	row_offsets = new (std::nothrow) int[n + 1];    // (n+1, &row_offset);
	column_indices = new (std::nothrow) int[nnz];
	matrix_values = new (std::nothrow) ValueType[nnz];

	//Importar matriz aca
	row_offsets[0] = 0;
	std::memset(row_offsets + 1, 1, n);

	Foam::UList<int>::const_iterator it_low =
			ref_foamMatrix.lduAddr().lowerAddr().begin();
	Foam::UList<int>::const_iterator it_up =
			ref_foamMatrix.lduAddr().upperAddr().begin();

	for (int i = 0; i < ref_foamMatrix.lower().size(); ++i) {
		row_offsets[*(it_low++) + 1]++;
		row_offsets[*(it_up++) + 1]++;
	}

	//CSR
	//TODO (juan) ver como se puede mejorar esto
	int sum = 0;
	for (int i = 0; i < n; ++i) {
		int temp = row_offsets[i];
		row_offsets[i] = sum;
		sum += temp;
	}
	row_offsets[n] = sum;

	Foam::UList<double>::const_iterator it_val = ref_foamMatrix.lower().begin();
	it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
	it_up = ref_foamMatrix.lduAddr().upperAddr().begin();

	// fill col and val arrays for lower part
	for (int i = 0; i < ref_foamMatrix.lower().size(); ++i) {
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
	for (int i = 0; i < ref_foamMatrix.diag().size(); ++i) {
		int dest_diag = row_offsets[i + 1];
		matrix_values[dest_diag] = *it_val;
		column_indices[dest_diag] = i;
		++row_offsets[i + 1];
		++it_val;
	}
	it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
	it_up = ref_foamMatrix.lduAddr().upperAddr().begin();
	it_val = ref_foamMatrix.upper().begin();

	// fill upper part
	for (int i = 0; i < ref_foamMatrix.upper().size(); ++i) {
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

	cl_int cl_status = CL_SUCCESS;

	// REVISAR: probablemente no funcione bien con el "ValueType"
	p_clSparseMatrix->values = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->num_nonzeros * sizeof(ValueType),
			NULL,
			&cl_status
		);

	p_clSparseMatrix->colIndices = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->num_nonzeros * sizeof(cl_int),
			NULL,
			&cl_status
		);

	p_clSparseMatrix->rowOffsets = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			(p_clSparseMatrix->num_rows + 1) * sizeof(cl_int),
			NULL,
			&cl_status
		);

	p_clSparseMatrix->rowBlocks = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->rowBlockSize * sizeof(cl_ulong),
			NULL,
			&cl_status
		);

	//OpenCL 2.0: usa clSVMAlloc <- para delegar la alocación de memoria en la GPU
	//FIXME!: ver como modificar cl_float/cl_double segun el TypeName
	clMemRAII<cl_double> rCsrValues(queue, p_clSparseMatrix->values);
	clMemRAII<cl_int> rCsrColIndices(queue, p_clSparseMatrix->colIndices);
	clMemRAII<cl_int> rCsrRowOffsets(queue, p_clSparseMatrix->rowOffsets);

	//FIXME! (juan) : ver como hacer cuando TypeName es float o es double
	//FIXME!: ver como modificar cl_float/cl_double segun el TypeName
	cl_double* fCsrValues = rCsrValues.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->valOffset*/,
			nnz
		);
	cl_int* iCsrColIndices = rCsrColIndices.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->colIndOffset*/,
			nnz
		);
	cl_int* iCsrRowOffsets = rCsrRowOffsets.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->rowOffOffset*/,
			p_clSparseMatrix->num_rows + 1
		);

	//Esto de puede mejorar al copiar directamente al espacio de memoria de la GPU.
	// Por ahora lo dejo así porque necesito probar que funcione correctamente.
	// TODO:(juan) refactorizar esto. Hacer la asignación directamente al copiar los valores desde openFOAM
	std::memcpy(fCsrValues, matrix_values, nnz);
	std::memcpy(iCsrColIndices, column_indices, nnz);
	std::memcpy(iCsrRowOffsets, row_offsets, n);

	// This function allocates memory for rowBlocks structure. If not called
	// the structure will not be calculated and clSPARSE will run the vectorized
	// version of SpMV instead of adaptive;
	clsparseCsrMetaSize(p_clSparseMatrix, control);
	p_clSparseMatrix->rowBlocks = ::clCreateBuffer(
			context,
			CL_MEM_READ_WRITE,
			p_clSparseMatrix->rowBlockSize * sizeof(cl_ulong),
			NULL,
			&cl_status
		);
	clsparseCsrMetaCompute(p_clSparseMatrix, control);

}

template<typename ValueType = double>
void importarVectorOpenFoam(const Foam::scalarField &foamVector, cldenseVector *vector, cl_context context, cl_command_queue queue) {
	size_t numeroElementos = (size_t) foamVector.size();
	cl_int cl_status = CL_SUCCESS;
	vector->values = clCreateBuffer(context, CL_MEM_READ_ONLY, numeroElementos, NULL, &cl_status);
	/**
	 * TODO (juan): Ver que es mas eficiente. Usar el mapeo de memoria de OpenCL 2.0 o copiar los datos directamente
	 *
	 */
	clMemRAII<cl_double> rValues(queue, vector->values);
	cl_double* fValues = rValues.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0,
			numeroElementos
		);
	std::copy(foamVector.begin(), foamVector.end(), fValues);
}

#endif // CLSPARSE_PCG_INIT_H
