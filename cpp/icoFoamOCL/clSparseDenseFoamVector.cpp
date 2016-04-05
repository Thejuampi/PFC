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
	void* d = (void*)foamVector.begin();
//	this->values = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, numeroElementos * sizeof(double), d, &cl_status);
	this->valuesMapper = new ValueMapper(queue, this->values, this->num_values);
//	int flag = permiteLectura ? CL_MAP_WRITE_INVALIDATE_REGION : CL_MAP_WRITE_INVALIDATE_REGION | CL_MAP_READ;
	cl_double* fValues = this->valuesMapper->clMapMem( CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, numeroElementos); //se agrega bandera para lectura, ver si funciona
	this->valuesMapper->clWriteMem(CL_TRUE, 0, numeroElementos, d);
	this->valuesMapper->clUnMapMem();
//	::clEnqueueCopyBuffer(queue, d, fValues, 0,0, numeroElementos*sizeof(double), );

}

void clSparseDenseFoamVector::exportar(Foam::scalarField& foamVector, cl_command_queue queue) {

	cl_int _clStatus = ::clEnqueueSVMMap( queue, CL_TRUE, CL_MAP_READ, this->valuesMapper->clMem, this->num_values * sizeof( cl_double), 0, NULL, NULL );
	if(_clStatus != CL_BUILD_SUCCESS) {
		exit(_clStatus);
	}
	cl_double* fValues = this->valuesMapper->clMem;
	double* data = foamVector.begin();
	for (int i = 0; i < this->num_values; ++i) {
		*data++ = fValues[i];
	}

	this->valuesMapper->clUnMapMem();

}

void clSparseDenseFoamVector::clear() {
    num_values = 0;
    values = nullptr;
    // ???
    valuesMapper = nullptr;
}

cl_ulong clSparseDenseFoamVector::offset() const {return 0;}

clSparseDenseFoamVector::~clSparseDenseFoamVector() {
	delete this->valuesMapper;
//	clReleaseMemObject(this->values);
}

