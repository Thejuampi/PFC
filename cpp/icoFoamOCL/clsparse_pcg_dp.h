#ifndef CLSPARSE_PCG_DP_H
#define CLSPARSE_PCG_DP_H

#include <clSPARSE.h>
#include <clSPARSE-2x.h>
#include <clSPARSE-2x.hpp>
#include <vector>

#include "/opt/intel/intel-opencl-1.2-5.0.0.43/opencl-1.2-sdk-5.0.0.43/include/CL/cl.hpp"
#include "/opt/openfoam240/src/finiteVolume/cfdTools/general/include/fvCFD.H"
#include "/opt/openfoam240/src/OpenFOAM/matrices/lduMatrix/lduMatrix/lduMatrix.H"

namespace Foam {

/*---------------------------------------------------------------------------*\
                           Class clSPARSE_PCG_DP Declaration
\*---------------------------------------------------------------------------*/

class clSPARSE_PCG_DP : public lduMatrix::solver
{
private:
    // Private Member Functions

        //- Disallow default bitwise copy construct
        clSPARSE_PCG_DP(const clSPARSE_PCG_DP&);

        //- Disallow default bitwise assignment
        void operator=(const clSPARSE_PCG_DP&);

        cl::Device g_device;
        cl::Platform g_platform;
        cl::CommandQueue g_queue;
        cl_int cl_status;
        std::vector<cl::Platform> g_platforms;
        std::vector<cl::Device> g_devices;

        /**
         * @brief Varibales de clSPARSE
         */
//        cldenseVector m_x;
//        cldenseVector g_b;
//        clsparseCsrMatrix g_A;
//        clsparseStatus status;
        clsparseControl g_clSparseControl;
        cl::Context g_context;

        /**
         *
         */
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
