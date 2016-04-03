/*
 * clSparseFoamMatrix.cpp
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#include "clSparseFoamMatrix.h"
#include <parallel/algorithm>

clSparseFoamMatrix::~clSparseFoamMatrix() {
	delete valuesMapper;
	delete columnsMapper;
	delete rowOffsetsMapper;


	//if(this->values)	 clSVMFree(this->values);
	//if(this->colIndices) clSVMFree(this->colIndices);
	//if(this->rowOffsets) clSVMFree(this->rowOffsets);
	//if(this->rowBlocks)  clSVMFree(this->rowBlocks);
}

clSparseFoamMatrix::clSparseFoamMatrix(const Foam::lduMatrix& ref_foamMatrix, cl_context context, cl_command_queue queue, clsparseControl control) :
		clsparseCsrMatrix_() {

	clsparseInitCsrMatrix(this);
	this->valuesMapper = NULL;
	this->columnsMapper = NULL;
	this->rowOffsetsMapper = NULL;

	// Matrix size
	int n = ref_foamMatrix.diag().size();
	int nnz = ref_foamMatrix.diag().size() + ref_foamMatrix.lower().size() + ref_foamMatrix.upper().size();

	// CSR values
	int *row_offsets = NULL;
	int *column_indices = NULL;
	double *matrix_values = NULL;

	row_offsets = new (std::nothrow) int[n + 1];
	column_indices = new (std::nothrow) int[nnz];
	matrix_values = new (std::nothrow) double[nnz];

	//Importar matriz aca
	row_offsets[0] = 0;
	std::fill_n(row_offsets + 1, n, 1);

	Foam::UList<int>::const_iterator it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
	Foam::UList<int>::const_iterator it_up = ref_foamMatrix.lduAddr().upperAddr().begin();

	for (int i = 0; i < ref_foamMatrix.lower().size(); ++i) {
		row_offsets[*(it_low++) + 1]++;
		row_offsets[*(it_up++) + 1]++;
	}

	//CSR
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

	this->num_nonzeros = nnz;
	this->num_cols = nnz;
	this->num_rows = n;

	cl_int cl_status = CL_SUCCESS;

	this->valuesMapper = new ValueMapper(queue, this->values, this->num_nonzeros);
	this->columnsMapper = new IndexMapper(queue, this->colIndices, this->num_nonzeros);
	this->rowOffsetsMapper = new IndexMapper(queue, this->rowOffsets, this->num_rows + 1);

	valuesMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, this->num_nonzeros, &cl_status);
	columnsMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, this->num_nonzeros, &cl_status);
	rowOffsetsMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, this->num_rows + 1, &cl_status);

	valuesMapper->clWriteMem(CL_TRUE, 0, this->num_nonzeros, (void*) matrix_values);
	columnsMapper->clWriteMem(CL_TRUE, 0, this->num_nonzeros, (void*) column_indices);
	rowOffsetsMapper->clWriteMem(CL_TRUE, 0, this->num_rows, (void*) row_offsets);

	//Es necesario desmapear la memoria para poder utilizarla en la GPU cuando no se utiliza fine grained svm
	valuesMapper->clUnMapMem();
	columnsMapper->clUnMapMem();
	rowOffsetsMapper->clUnMapMem();

	delete matrix_values;
	delete column_indices;
	delete row_offsets;

}
