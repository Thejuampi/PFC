/*
 * clSparseFoamMatrix.h
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#ifndef CLSPARSEFOAMMATRIX_H_
#define CLSPARSEFOAMMATRIX_H_

#include <CL/cl.h>
#include <clSPARSE.h>
#include "clSparseUtils.h"

typedef clMemRAII<cl_double> clMemMapper;

/**
 * author: jpalescano
 */
class clSparseFoamMatrix: public clsparseCsrMatrix_ {
private:

	clMemMapper *memoryMapper;

public:
	clSparseFoamMatrix();
	virtual ~clSparseFoamMatrix();

	void setMemoryMapper(clMemMapper* memMapper);

};

#endif /* CLSPARSEFOAMMATRIX_H_ */
