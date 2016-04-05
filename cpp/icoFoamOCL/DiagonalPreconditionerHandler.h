/*
 * DiagonalPreconditionerHandler.h
 *
 *  Created on: 5 de abr. de 2016
 *      Author: juan
 */

#ifndef DIAGONALPRECONDITIONERHANDLER_H_
#define DIAGONALPRECONDITIONERHANDLER_H_

class DiagonalPreconditionerHandler : public ClPreconditionerHandler {
public:
	DiagonalPreconditionerHandler();
	virtual ~DiagonalPreconditionerHandler();
};

#endif /* DIAGONALPRECONDITIONERHANDLER_H_ */
