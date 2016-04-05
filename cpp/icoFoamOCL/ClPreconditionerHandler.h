/*
 * ClPreconditionerHandler.h
 *
 *  Created on: 4 de abr. de 2016
 *      Author: juan
 */

#ifndef CLPRECONDITIONERHANDLER_H_
#define CLPRECONDITIONERHANDLER_H_

//template<typename T>
class ClPreconditionerHandler {
public:
	ClPreconditionerHandler();

	virtual void operator()(const clSparseDenseFoamVector& x, clSparseDenseFoamVector& y, clsparseControl control) = 0;
	virtual void notify(const clSparseFoamMatrix* pA, clsparseControl control) = 0;

	virtual ~ClPreconditionerHandler();
};

#endif /* CLPRECONDITIONERHANDLER_H_ */
