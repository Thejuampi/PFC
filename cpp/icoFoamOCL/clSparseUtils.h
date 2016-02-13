#ifndef CLSPARSE_PCG_INIT_H
#define CLSPARSE_PCG_INIT_H


#include "lduMatrix.H"
#include <CL/cl.hpp>
#include "clSPARSE.h"
//#include <clSPARSE.h">

template< typename pType >
class clMemRAII
{
    cl_command_queue clQueue;
    pType* clMem;
    cl_bool clOwner;

public:

    //temporary solution for situations when clMemRaII is allocating buffer,
    // bug should not release it when calling the destructor.
    // ProperFIX: write operator=(const clMemRAII&)
    clMemRAII( const cl_command_queue cl_queue, void** cl_malloc,
               const size_t cl_size = 0, const cl_svm_mem_flags cl_flags = CL_MEM_READ_WRITE):
        clMem( nullptr ), clOwner(false)
    {
        clQueue = cl_queue;
        clMem = static_cast< pType* >( *cl_malloc );

        if(cl_size > 0)
        {
            cl_context ctx = NULL;

            ::clGetCommandQueueInfo(clQueue, CL_QUEUE_CONTEXT, sizeof( cl_context ), &ctx, NULL);
            cl_int status = 0;

            clMem = static_cast< pType* > (clSVMAlloc(ctx, cl_flags, cl_size * sizeof(pType), 0));
            *cl_malloc = clMem;
        }

        ::clRetainCommandQueue( clQueue );
    }

    clMemRAII( const cl_command_queue cl_queue, void* cl_malloc,
               const size_t cl_size = 0, const cl_svm_mem_flags cl_flags = CL_MEM_READ_WRITE):
        clMem( nullptr ), clOwner(false)
    {
        clQueue = cl_queue;
        clMem = static_cast< pType* >( cl_malloc );

        if(cl_size > 0)
        {
            cl_context ctx = NULL;

            ::clGetCommandQueueInfo(clQueue, CL_QUEUE_CONTEXT, sizeof( cl_context ), &ctx, NULL);
            cl_int status = 0;

            clMem = static_cast< pType* > (clSVMAlloc(ctx, cl_flags, cl_size * sizeof(pType), 0));
            clOwner = true;
        }

        ::clRetainCommandQueue( clQueue );
    }

    pType* clMapMem( cl_bool clBlocking, const cl_map_flags clFlags, const size_t clOff, const size_t clSize, cl_int *clStatus = nullptr)
    {
        // Right now, we don't support returning an event to wait on
        clBlocking = CL_TRUE;

        cl_int _clStatus = ::clEnqueueSVMMap( clQueue, clBlocking, clFlags,
                                              clMem, clSize * sizeof( pType ), 0, NULL, NULL );
        if (clStatus != nullptr)
        {
            *clStatus = _clStatus;
        }

        return clMem;
    }

    void clWriteMem( cl_bool clBlocking, const size_t clOff, const size_t clSize, const void* srcPtr )
    {
        // Right now, we don't support returning an event to wait on
        clBlocking = CL_TRUE;

        cl_int clStatus = ::clEnqueueSVMMemcpy( clQueue, clBlocking, clMem, srcPtr,
                                                  clSize * sizeof( pType ), 0, NULL, NULL );
    }

    void clFillMem (const pType pattern, const size_t clOff, const size_t clSize)
    {
        cl_int clStatus = ::clEnqueueSVMMemFill(clQueue, clMem,
                                                &pattern, sizeof(pType),
                                                clSize * sizeof(pType),
                                                0, NULL, NULL);
    }

    ~clMemRAII( )
    {
        if( clMem )
            ::clEnqueueSVMUnmap( clQueue, clMem, 0, NULL, NULL );

        if(clOwner)
        {
            cl_context ctx = nullptr;
            ::clGetCommandQueueInfo( clQueue, CL_QUEUE_CONTEXT, sizeof( cl_context ), &ctx, NULL);
            ::clSVMFree(ctx, clMem);
        }

        ::clReleaseCommandQueue( clQueue );
    }
};


//template<typename ValueType = double>
void importarMatrizDP(const Foam::lduMatrix &ref_foamMatrix, clsparseCsrMatrix *p_clSparseMatrix, cl_context context, cl_command_queue queue, clsparseControl control) {

	//TODO (juan) ver si esto es necesario cada ves, o si se puede "reutilizar" el espacio
	clsparseInitCsrMatrix(p_clSparseMatrix);

	// Matrix size

	int n = ref_foamMatrix.diag().size();
	int nnz = ref_foamMatrix.diag().size() + ref_foamMatrix.lower().size()
			+ ref_foamMatrix.upper().size();

	// CSR values
	int *row_offsets = NULL;
	int *column_indices = NULL;
	double *matrix_values = NULL;

	// reservar lugar:
	//TODO (juan) : ver si se puede hacer directamente en la memoria de la GPU
	row_offsets = new (std::nothrow) int[n + 1];    // (n+1, &row_offset);
	column_indices = new (std::nothrow) int[nnz];
	matrix_values = new (std::nothrow) double[nnz];

	//Importar matriz aca
	row_offsets[0] = 0;
	std::memset(row_offsets + 1, 1, n);

	Foam::UList<int>::const_iterator it_low =
			ref_foamMatrix.lduAddr().lowerAddr().begin();
	Foam::UList<int>::const_iterator it_up =
			ref_foamMatrix.lduAddr().upperAddr().begin();

	for (int i = 0; i < ref_foamMatrix.lower().size(); ++i) {
		row_offsets[*(it_low++) + 1]++;
		row_offsets[*(it_up++) + 1]++;
	}

	//CSR
	//TODO (juan) ver como se puede mejorar esto
	int sum = 0;
	for (int i = 0; i < n; ++i) {
		int temp = row_offsets[i];
		row_offsets[i] = sum;
		sum += temp;
	}
	row_offsets[n] = sum;

	Foam::UList<double>::const_iterator it_val = ref_foamMatrix.lower().begin();
	it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
	it_up = ref_foamMatrix.lduAddr().upperAddr().begin();

	// fill col and val arrays for lower part
	for (int i = 0; i < ref_foamMatrix.lower().size(); ++i) {
		// row index for lower = upper + 1
		int r_lower = *it_up + 1;
		int dest_lower = row_offsets[r_lower];

		column_indices[dest_lower] = *it_low;
		matrix_values[dest_lower] = *it_val;
		++row_offsets[r_lower];

		++it_low;
		++it_up;
		++it_val;
	}

	// fill diagonal part
	it_val = ref_foamMatrix.diag().begin();
	for (int i = 0; i < ref_foamMatrix.diag().size(); ++i) {
		int dest_diag = row_offsets[i + 1];
		matrix_values[dest_diag] = *it_val;
		column_indices[dest_diag] = i;
		++row_offsets[i + 1];
		++it_val;
	}
	it_low = ref_foamMatrix.lduAddr().lowerAddr().begin();
	it_up = ref_foamMatrix.lduAddr().upperAddr().begin();
	it_val = ref_foamMatrix.upper().begin();

	// fill upper part
	for (int i = 0; i < ref_foamMatrix.upper().size(); ++i) {
		// row index for upper part = lower + 1
		int r_upper = *it_low + 1;
		int dest_upper = row_offsets[r_upper];

		column_indices[dest_upper] = *it_up;
		matrix_values[dest_upper] = *it_val;
		++row_offsets[r_upper];

		++it_low;
		++it_up;
		++it_val;
	}

	p_clSparseMatrix->num_nonzeros = nnz;
	p_clSparseMatrix->num_cols = n;
	p_clSparseMatrix->num_rows = n;

	cl_int cl_status = CL_SUCCESS;

	// REVISAR: probablemente no funcione bien con el "ValueType"
	p_clSparseMatrix->values = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->num_nonzeros * sizeof(double),
			NULL,
			&cl_status
		);

	p_clSparseMatrix->colIndices = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->num_nonzeros * sizeof(cl_int),
			NULL,
			&cl_status
		);

	p_clSparseMatrix->rowOffsets = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			(p_clSparseMatrix->num_rows + 1) * sizeof(cl_int),
			NULL,
			&cl_status
		);

	p_clSparseMatrix->rowBlocks = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->rowBlockSize * sizeof(cl_ulong),
			NULL,
			&cl_status
		);

	//OpenCL 2.0: usa clSVMAlloc <- para delegar la alocación de memoria en la GPU
	//FIXME!: ver como modificar cl_float/cl_double segun el TypeName
	clMemRAII<cl_double> rCsrValues(queue, p_clSparseMatrix->values);
	clMemRAII<cl_int> rCsrColIndices(queue, p_clSparseMatrix->colIndices);
	clMemRAII<cl_int> rCsrRowOffsets(queue, p_clSparseMatrix->rowOffsets);

	//FIXME! (juan) : ver como hacer cuando TypeName es float o es double
	//FIXME!: ver como modificar cl_float/cl_double segun el TypeName
	cl_double* fCsrValues = rCsrValues.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->valOffset*/,
			nnz
		);
	cl_int* iCsrColIndices = rCsrColIndices.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->colIndOffset*/,
			nnz
		);
	cl_int* iCsrRowOffsets = rCsrRowOffsets.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->rowOffOffset*/,
			p_clSparseMatrix->num_rows + 1
		);

	//Esto de puede mejorar al copiar directamente al espacio de memoria de la GPU.
	// Por ahora lo dejo así porque necesito probar que funcione correctamente.
	// TODO:(juan) refactorizar esto. Hacer la asignación directamente al copiar los valores desde openFOAM
	std::memcpy(fCsrValues, matrix_values, nnz);
	std::memcpy(iCsrColIndices, column_indices, nnz);
	std::memcpy(iCsrRowOffsets, row_offsets, n);

	// This function allocates memory for rowBlocks structure. If not called
	// the structure will not be calculated and clSPARSE will run the vectorized
	// version of SpMV instead of adaptive;
	clsparseCsrMetaSize(p_clSparseMatrix, control);
	p_clSparseMatrix->rowBlocks = ::clCreateBuffer(
			context,
			CL_MEM_READ_WRITE,
			p_clSparseMatrix->rowBlockSize * sizeof(cl_ulong),
			NULL,
			&cl_status
		);
	clsparseCsrMetaCompute(p_clSparseMatrix, control);

}

//template<typename ValueType = double>
void importarVectorOpenFoam(const Foam::scalarField &foamVector, cldenseVector *vector, cl_context context, cl_command_queue queue) {
	auto numeroElementos = foamVector.size();
	cl_int cl_status = CL_SUCCESS;
	vector->values = clCreateBuffer(context, CL_MEM_READ_ONLY, numeroElementos, NULL, &cl_status);
	/**
	 * TODO (juan): Ver que es mas eficiente. Usar el mapeo de memoria de OpenCL 2.0 o copiar los datos directamente
	 *
	 */
	clMemRAII<cl_double> rValues(queue, vector->values);
	cl_double* fValues = rValues.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0,
			numeroElementos
		);

	long idx = 0;
	for(auto it = foamVector.begin(); it != foamVector.end(); ++it ) {
		fValues[idx++] = *it;
	}
//	std::copy(foamVector.begin(), foamVector.end(), fValues);
}

#endif // CLSPARSE_PCG_INIT_H
