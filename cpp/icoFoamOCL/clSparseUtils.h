#ifndef CLSPARSE_PCG_INIT_H
#define CLSPARSE_PCG_INIT_H


#include "lduMatrix.H"
#include <CL/cl.h>
#include <CL/cl.hpp>
#include "clSPARSE.h"
#include <vector>
//#include <clSPARSE.h">

#define DD_UTIL

void info(std::string msj) {
#ifdef DD_UTIL
	Foam::Info << "[INFO] - " << msj <<"\n";
#endif
}

template<class M>
void info(M& m) {
#ifdef DD_UTIL
	std::stringstream ss;
	ss << m;
	info(ss.str());
#endif
}


template< typename pType=double >
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
    	//FIXME! : al destruirse genera un segmentation fault: corregir o va a quedar un memory leak!
//        if( clMem )
//            ::clEnqueueSVMUnmap( clQueue, clMem, 0, NULL, NULL );
//
//        if(clOwner)
//        {
//            cl_context ctx = nullptr;
//            ::clGetCommandQueueInfo( clQueue, CL_QUEUE_CONTEXT, sizeof( cl_context ), &ctx, NULL);
//            ::clSVMFree(ctx, clMem);
//        }
//
//        ::clReleaseCommandQueue( clQueue );
    }
};


std::string codes[] = {
	"CL_SUCCESS",
	"CL_DEVICE_NOT_FOUND",
	"CL_DEVICE_NOT_AVAILABLE",
	"CL_COMPILER_NOT_AVAILABLE",
	"CL_MEM_OBJECT_ALLOCATION_FAILURE",
	"CL_OUT_OF_RESOURCES",
	"CL_OUT_OF_HOST_MEMORY",
	"CL_PROFILING_INFO_NOT_AVAILABLE",
	"CL_MEM_COPY_OVERLAP",
	"CL_IMAGE_FORMAT_MISMATCH",
	"CL_IMAGE_FORMAT_NOT_SUPPORTED",
	"CL_BUILD_PROGRAM_FAILURE",
	"CL_MAP_FAILURE",
	"CL_MISALIGNED_SUB_BUFFER_OFFSET",
	"CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST",
	"CL_COMPILE_PROGRAM_FAILURE",
	"CL_LINKER_NOT_AVAILABLE",
	"CL_LINK_PROGRAM_FAILURE",
	"CL_DEVICE_PARTITION_FAILED",
	"CL_KERNEL_ARG_INFO_NOT_AVAILABLE",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"NULL",
	"CL_INVALID_VALUE",
	"CL_INVALID_DEVICE_TYPE",
	"CL_INVALID_PLATFORM",
	"CL_INVALID_DEVICE",
	"CL_INVALID_CONTEXT",
	"CL_INVALID_QUEUE_PROPERTIES",
	"CL_INVALID_COMMAND_QUEUE",
	"CL_INVALID_HOST_PTR",
	"CL_INVALID_MEM_OBJECT",
	"CL_INVALID_IMAGE_FORMAT_DESCRIPTO",
	"CL_INVALID_IMAGE_SIZE",
	"CL_INVALID_SAMPLER",
	"CL_INVALID_BINARY",
	"CL_INVALID_BUILD_OPTIONS",
	"CL_INVALID_PROGRAM",
	"CL_INVALID_PROGRAM_EXECUTABLE",
	"CL_INVALID_KERNEL_NAME",
	"CL_INVALID_KERNEL_DEFINITION",
	"CL_INVALID_KERNEL",
	"CL_INVALID_ARG_INDEX",
	"CL_INVALID_ARG_VALUE",
	"CL_INVALID_ARG_SIZE",
	"CL_INVALID_KERNEL_ARGS",
	"CL_INVALID_WORK_DIMENSION",
	"CL_INVALID_WORK_GROUP_SIZE",
	"CL_INVALID_WORK_ITEM_SIZE",
	"CL_INVALID_GLOBAL_OFFSET",
	"CL_INVALID_EVENT_WAIT_LIST",
	"CL_INVALID_EVENT",
	"CL_INVALID_OPERATION",
	"CL_INVALID_GL_OBJECT",
	"CL_INVALID_BUFFER_SIZE",
	"CL_INVALID_MIP_LEVEL",
	"CL_INVALID_GLOBAL_WORK_SIZE",
	"CL_INVALID_PROPERTY",
	"CL_INVALID_IMAGE_DESCRIPTOR"   ,
	"CL_INVALID_COMPILER_OPTIONS"    ,
	"CL_INVALID_LINKER_OPTIONS"       ,
	"CL_INVALID_DEVICE_PARTITION_COUNT",
	"CL_INVALID_PIPE_SIZE"             ,
	"CL_INVALID_DEVICE_QUEUE"
};

void verificarError(int code) {

	if(code != CL_SUCCESS) {
		try {
			std::string& error_description = codes[-code];
			Foam::Info << "[Error] CODE:" << code << " = " << codes[-code] <<"\n";
		} catch(std::exception& e){
			Foam::Info << "Excepción:" << e.what();
		}
		exit(code);
	}

}

template<class REAL_TYPE>
void ldu2csr(
							const Foam::lduMatrix           & matrix,
										 std::vector< int >       & c_idx,
										 std::vector< int >       & r_idx,
										 std::vector< REAL_TYPE > & vals
						)
{
	int n_rows = matrix.diag().size() ;

	for (int i=0; i < matrix.upper().size() ; i++) {
		int ri1 = matrix.lduAddr().lowerAddr()[i] ;
		int ri2 = matrix.lduAddr().upperAddr()[i] ;
		r_idx[ri1+2] ++ ;
		r_idx[ri2+2] ++ ;
	} ;

	r_idx[0] = 0 ;
	r_idx[1] = 0 ;
	for(int i=1; i<n_rows; i++) {
		r_idx[i+1] += r_idx[i] ;
	} ;

	// lower triangle
	for (int i=0; i < matrix.lower().size() ; i++) {
		int row    = matrix.lduAddr().upperAddr()[i] +1;
		int column = matrix.lduAddr().lowerAddr()[i] ;

		int idx = r_idx[row] ;
		vals[idx] = matrix.lower()[i] ;
		c_idx[idx] = column ;
		r_idx[row]++ ;
	} ;
	// diagonal
	for (int i=0; i<matrix.diag().size(); i++) {
		int idx = r_idx[i+1] ;
		vals[idx] = matrix.diag()[i] ;
		c_idx[idx] = i ; // i is row and column index
		r_idx[i+1]++ ;
	} ;
	// upper triangle
	for (int i=0; i < matrix.upper().size() ; i++) {
		int row    = matrix.lduAddr().lowerAddr()[i] +1;
		int column = matrix.lduAddr().upperAddr()[i] ;

		int idx = r_idx[row] ;
		vals[idx] = matrix.upper()[i] ;
		c_idx[idx] = column ;
		r_idx[row]++ ;
	} ;
} ;

void importarMatrizDP2(const Foam::lduMatrix &matrix, clsparseCsrMatrix *p_clSparseMatrix, cl_context context, cl_command_queue queue, clsparseCreateResult createResult) {
	int n_rows = matrix.diag().size() ;
    int nnz = matrix.lower().size() + matrix.upper().size() + matrix.diag().size() ;

    std::vector<double> vals(nnz) ;
    std::vector<int> c_idx(nnz) ;
    std::vector<int> r_idx(n_rows+2, 1);

	ldu2csr(matrix, c_idx, r_idx, vals);


}


//template<typename ValueType = double>
void importarMatrizDP(const Foam::lduMatrix &ref_foamMatrix, clsparseCsrMatrix *p_clSparseMatrix, cl_context context, cl_command_queue queue, clsparseCreateResult createResult) {

	//TODO (juan) ver si esto es necesario cada ves, o si se puede "reutilizar" el espacio

//	clsparseInitCsrMatrix(p_clSparseMatrix);

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

//	std::memset(row_offsets + 1, 1, n);
	std::fill_n(row_offsets, n, 1);

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
//	verificarError(cl_status);

	p_clSparseMatrix->col_indices = ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			p_clSparseMatrix->num_nonzeros * sizeof(cl_int),
			NULL,
			&cl_status
		);
//	verificarError(cl_status);

	p_clSparseMatrix->row_pointer= ::clCreateBuffer(
			context,
			CL_MEM_READ_ONLY,
			(p_clSparseMatrix->num_rows + 1) * sizeof(cl_int),
			NULL,
			&cl_status
		);
//	verificarError(cl_status);

//	p_clSparseMatrix->rowBlocks = ::clCreateBuffer(
//			context,
//			CL_MEM_READ_ONLY,
//			p_clSparseMatrix->rowBlockSize * sizeof(cl_ulong),
//			NULL,
//			&cl_status
//		);
//	verificarError(cl_status);

	//OpenCL 2.0: usa clSVMAlloc <- para delegar la alocación de memoria en la GPU
	//FIXME!: ver como modificar cl_float/cl_double segun el TypeName

	clMemRAII<cl_double> rCsrValues(queue, p_clSparseMatrix->values, p_clSparseMatrix->num_nonzeros);
	clMemRAII<cl_int> rCsrColIndices(queue, p_clSparseMatrix->col_indices, p_clSparseMatrix->num_nonzeros);
	clMemRAII<cl_int> rCsrRowOffsets(queue, p_clSparseMatrix->row_pointer, p_clSparseMatrix->num_rows+1); //???

	//FIXME! (juan) : ver como hacer cuando TypeName es float o es double
	//FIXME!: ver como modificar cl_float/cl_double segun el TypeName

	cl_double* fCsrValues = rCsrValues.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->valOffset*/,
			nnz,
			&cl_status
		);
//	verificarError(cl_status);
	cl_int* iCsrColIndices = rCsrColIndices.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->colIndOffset*/,
			nnz,
			&cl_status
		);
//	verificarError(cl_status);
	cl_int* iCsrRowOffsets = rCsrRowOffsets.clMapMem(
			CL_TRUE,
			CL_MAP_WRITE_INVALIDATE_REGION,
			0 /*p_clSparseMatrix->rowOffOffset*/,
			p_clSparseMatrix->num_rows + 1,
			&cl_status
		);
	verificarError(cl_status);

	// TODO:(juan) refactorizar esto. Hacer la asignación directamente al copiar los valores desde openFOAM

	for(size_t i = 0; i < nnz ; ++i) {
//		info(i);
		fCsrValues[i] = matrix_values[i];
		iCsrColIndices[i] = column_indices[i];
	}
	for(size_t i = 0; i < n; ++i) {
		iCsrRowOffsets[i] = row_offsets[i];
	}

//	clsparseCsrMetaSize(p_clSparseMatrix, control);
//	info("p_clSparseMatrix->rowBlocks = ::clCreateBuffer()");
//	p_clSparseMatrix->row_pointer= ::clCreateBuffer(
//			context,
//			CL_MEM_READ_WRITE,
//			p_clSparseMatrix->rowBlockSize * sizeof(cl_ulong),
//			NULL,
//			&cl_status
//		);
//	verificarError(cl_status);
//	info("clsparseCsrMetaCompute(p_clSparseMatrix, control)");
	clsparseCsrMetaCreate(p_clSparseMatrix, createResult.control);
}

//template<typename ValueType = double>
void importarVectorOpenFoam(const Foam::scalarField &foamVector, cldenseVector *vector, cl_context context, cl_command_queue queue) {
	size_t numeroElementos = (size_t)foamVector.size();
	cl_int cl_status = CL_SUCCESS;
	vector->values = clCreateBuffer(context, CL_MEM_READ_ONLY, numeroElementos, NULL, &cl_status);
	 //TODO (juan): Ver que es mas eficiente. Usar el mapeo de memoria de OpenCL 2.0 o copiar los datos directamente

	clMemRAII<cl_double> rValues(queue, vector->values,numeroElementos);
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
}

#endif // CLSPARSE_PCG_INIT_H
