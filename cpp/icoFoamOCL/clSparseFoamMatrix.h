/*
 * clSparseFoamMatrix.h
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#ifndef CLSPARSEFOAMMATRIX_H_
#define CLSPARSEFOAMMATRIX_H_

//#include <CL/cl.h>
#include <clSPARSE.h>
#include "clMemMapper.h"
#include "lduMatrix.H"

typedef clMemMapper<cl_double> ValueMapper;
typedef clMemMapper<cl_int> IndexMapper;

class clSparseFoamMatrix: public clsparseCsrMatrix_ {
private:

	ValueMapper *valuesMapper;
	IndexMapper *columnsMapper;
	IndexMapper *rowOffsetsMapper;
	void ldu2csr(const Foam::lduMatrix & matrix, cl_int* c_idx, cl_int *r_idx, cl_double *vals);

public:

	clSparseFoamMatrix(const Foam::lduMatrix &ref_foamMatrix, cl_context context, cl_command_queue queue, clsparseControl control);

    void clear( );
    cl_uint nnz_per_row() const;
    cl_ulong valOffset () const;
    cl_ulong colIndOffset () const;
    cl_ulong rowOffOffset () const;
    cl_ulong rowBlocksOffset( ) const;

	virtual ~clSparseFoamMatrix();

};

#endif /* CLSPARSEFOAMMATRIX_H_ */
