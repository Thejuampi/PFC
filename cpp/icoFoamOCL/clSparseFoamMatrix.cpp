/*
 * clSparseFoamMatrix.cpp
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#include "clSparseFoamMatrix.h"

clSparseFoamMatrix::clSparseFoamMatrix() : clsparseCsrMatrix_(){
	this->memoryMapper = NULL;
}

clSparseFoamMatrix::~clSparseFoamMatrix() {
	// TODO Auto-generated destructor stub
}

void clSparseFoamMatrix::setMemoryMapper(clMemMapper* memMapper) {
	this->memoryMapper = memMapper;
}
