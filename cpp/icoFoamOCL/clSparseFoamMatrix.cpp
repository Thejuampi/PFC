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
}

void clSparseFoamMatrix::ldu2csr(const Foam::lduMatrix & matrix, cl_int* c_idx, cl_int *r_idx, cl_double *vals) {
	int n_rows = matrix.diag().size();
	for (int i = 0; i < matrix.upper().size(); i++) {
		int ri1 = matrix.lduAddr().lowerAddr()[i];
		int ri2 = matrix.lduAddr().upperAddr()[i];
		r_idx[ri1 + 2]++;
		r_idx[ri2 + 2]++;
	};

	r_idx[0] = 0;
	r_idx[1] = 0;
	for (int i = 1; i < n_rows; i++) {
		r_idx[i + 1] += r_idx[i];
	};

	// lower triangle
	for (int i = 0; i < matrix.lower().size(); i++) {
		int row = matrix.lduAddr().upperAddr()[i] + 1;
		int column = matrix.lduAddr().lowerAddr()[i];

		int idx = r_idx[row];
		vals[idx] = matrix.lower()[i];
		c_idx[idx] = column;
		r_idx[row]++;
	};
	// diagonal
	for (int i = 0; i < matrix.diag().size(); i++) {
		int idx = r_idx[i + 1];
		vals[idx] = matrix.diag()[i];
		c_idx[idx] = i; // i is row and column index
		r_idx[i + 1]++;
	};
	// upper triangle
	for (int i = 0; i < matrix.upper().size(); i++) {
		int row = matrix.lduAddr().lowerAddr()[i] + 1;
		int column = matrix.lduAddr().upperAddr()[i];

		int idx = r_idx[row];
		vals[idx] = matrix.upper()[i];
		c_idx[idx] = column;
		r_idx[row]++;
	};
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

	this->num_nonzeros = nnz;
	this->num_cols = nnz;
	this->num_rows = n;

	cl_int cl_status = CL_SUCCESS;

	this->valuesMapper = new ValueMapper(queue, this->values, this->num_nonzeros);
	this->columnsMapper = new IndexMapper(queue, this->colIndices, this->num_nonzeros);
	this->rowOffsetsMapper = new IndexMapper(queue, this->rowOffsets, this->num_rows + 1);

	this->valuesMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, this->num_nonzeros, &cl_status);
	this->columnsMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, this->num_nonzeros, &cl_status);
	this->rowOffsetsMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, this->num_rows + 1, &cl_status);

	this->ldu2csr(ref_foamMatrix, columnsMapper->clMem, rowOffsetsMapper->clMem, valuesMapper->clMem);

	//Es necesario desmapear la memoria para poder utilizarla en la GPU cuando no se utiliza fine grained svm
	this->valuesMapper->clUnMapMem();
	this->columnsMapper->clUnMapMem();
	this->rowOffsetsMapper->clUnMapMem();

}
