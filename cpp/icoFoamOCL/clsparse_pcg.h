#ifndef CLSPARSE_PCG_H
#define CLSPARSE_PCG_H

#include "lduMatrix.H"
#include "clSparseUtils.h"

namespace Foam {

/*---------------------------------------------------------------------------*\
                           Class clSPARSE_PCG Declaration
\*---------------------------------------------------------------------------*/

class clSPARSE_PCG : public lduMatrix::solver
{
private:
    // Private Member Functions

        //- Disallow default bitwise copy construct
        clSPARSE_PCG(const clSPARSE_PCG&);

        //- Disallow default bitwise assignment
        void operator=(const clSPARSE_PCG&);

public:

    //- Runtime type information
    TypeName("clSPARCE_PCG")

    clSPARSE_PCG(
        const word& fieldName,
        const lduMatrix& matrix,
        const FieldField<Field, scalar>& interfaceBouCoeffs,
        const FieldField<Field, scalar>& interfaceIntCoeffs,
        const lduInterfaceFieldPtrsList& interfaces,
        const dictionary& solverControls
    );

    //- Destructor
    virtual ~clSPARSE_PCG()
    {}

    virtual solverPerformance solve
    (
        scalarField& psi,
        const scalarField& source,
        const direction cmpt=0
    ) const;
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif // CLSPARSE_PCG_H

// ************************************************************************* //
