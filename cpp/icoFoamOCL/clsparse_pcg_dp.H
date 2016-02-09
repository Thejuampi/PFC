#ifndef CLSPARSE_PCG_DP_H
#define CLSPARSE_PCG_DP_H

#include "lduMatrix.H"
#include <vector>

#include <CL/cl.hpp>
#include <clSPARSE.h>

namespace Foam {

/*---------------------------------------------------------------------------*\
                           Class clSPARSE_PCG_DP Declaration
\*---------------------------------------------------------------------------*/

class clSPARSE_PCG_DP
:
	public lduMatrix::solver
{
private:
    // Private Member Functions

        //- Disallow default bitwise copy construct
        clSPARSE_PCG_DP(const clSPARSE_PCG_DP&);

        //- Disallow default bitwise assignment
        void operator=(const clSPARSE_PCG_DP&);

        cl::Device m_device;
        cl::Platform m_platform;
        cl::CommandQueue m_queue;
        cl_int cl_status;
        std::vector<cl::Platform> m_platforms;
        std::vector<cl::Device> g_devices;

        clsparseControl m_clSparseControl;
        cl::Context m_context;

        cl_platform_id getPlatformId();
        cl_device_id getDeviceId();

public:

    //- Runtime type information
    TypeName("clSPARCE_PCG_DP")

    clSPARSE_PCG_DP(
        const word& fieldName,
        const lduMatrix& matrix,
        const FieldField <Field, scalar>& interfaceBouCoeffs,
        const FieldField<Field, scalar>& interfaceIntCoeffs,
        const lduInterfaceFieldPtrsList& interfaces,
        const dictionary& solverControls
    );

    //- Destructor
    virtual ~clSPARSE_PCG_DP()
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

#endif // CLSPARSE_PCG_DP_H

// ************************************************************************* //
