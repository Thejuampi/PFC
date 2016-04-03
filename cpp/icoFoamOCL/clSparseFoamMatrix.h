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

public:

	clSparseFoamMatrix(const Foam::lduMatrix &ref_foamMatrix, cl_context context, cl_command_queue queue, clsparseControl control);

	virtual ~clSparseFoamMatrix();

};

#endif /* CLSPARSEFOAMMATRIX_H_ */
