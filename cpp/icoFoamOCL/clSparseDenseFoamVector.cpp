/*
 * clSparseDenseFoamVector.cpp
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#include "clSparseDenseFoamVector.h"

clSparseDenseFoamVector::clSparseDenseFoamVector(const Foam::scalarField& foamVector, cl_context context, cl_command_queue queue, bool permiteLectura) {
	clsparseInitVector(this);
	this->valuesMapper = NULL;

	auto numeroElementos = foamVector.size();
	cl_int cl_status = CL_SUCCESS;
	this->num_values = numeroElementos;
	this->values = clCreateBuffer(context, CL_MEM_READ_WRITE, numeroElementos * sizeof(double), NULL, &cl_status);
	this->valuesMapper = new ValueMapper(queue, this->values, this->num_values);
	int flag = permiteLectura ? CL_MAP_WRITE_INVALIDATE_REGION : CL_MAP_WRITE_INVALIDATE_REGION | CL_MAP_READ;
	cl_double* fValues = this->valuesMapper->clMapMem( CL_TRUE, flag, 0, numeroElementos); //se agrega bandera para lectura, ver si funciona
	const void* d = foamVector.begin();
	this->valuesMapper->clWriteMem(CL_TRUE, 0, numeroElementos, d);

	//double *values = foamVector.data();
//	long idx = 0;
//	for (auto it = foamVector.begin(); it != foamVector.end(); ++it) {
//		fValues[idx++] = *it;
//	}

}

clSparseDenseFoamVector::~clSparseDenseFoamVector() {
	delete this->valuesMapper;
}

