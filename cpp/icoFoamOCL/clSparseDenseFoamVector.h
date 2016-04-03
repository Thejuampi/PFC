/*
 * clSparseDenseFoamVector.h
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#ifndef CLSPARSEDENSEFOAMVECTOR_H_
#define CLSPARSEDENSEFOAMVECTOR_H_

#include <clSPARSE.h>
#include "clMemMapper.h"
#include "lduMatrix.H"

typedef clMemMapper<cl_double> ValueMapper;

class clSparseDenseFoamVector: public cldenseVector_ {

private:
	ValueMapper* valuesMapper;


public:


	clSparseDenseFoamVector(const Foam::scalarField &foamVector, cl_context context, cl_command_queue queue, bool permiteLectura);

	void exportar(Foam::scalarField &foamVector);

	virtual ~clSparseDenseFoamVector();
};

#endif /* CLSPARSEDENSEFOAMVECTOR_H_ */
