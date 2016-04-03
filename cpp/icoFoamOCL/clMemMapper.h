/*
 * clMemMapper.h
 *
 *  Created on: 3 de abr. de 2016
 *      Author: juan
 */

#ifndef CLMEMMAPPER_H_
#define CLMEMMAPPER_H_

#include <CL/cl.h>

template<typename pType>
class clMemMapper {

private:
	cl_command_queue clQueue;
	cl_bool clOwner;

public:
	pType* clMem;

	clMemMapper(const cl_command_queue cl_queue, void** cl_malloc, const size_t cl_size = 0, const cl_svm_mem_flags cl_flags = CL_MEM_READ_WRITE);
	clMemMapper( const cl_command_queue cl_queue, void* cl_malloc, const size_t cl_size = 0, const cl_svm_mem_flags cl_flags = CL_MEM_READ_WRITE);
	pType* clMapMem( cl_bool clBlocking, const cl_map_flags clFlags, const size_t clOff, const size_t clSize, cl_int *clStatus = nullptr);
	void clWriteMem( cl_bool clBlocking, const size_t clOff, const size_t clSize, const void* srcPtr );
	void clFillMem (const pType pattern, const size_t clOff, const size_t clSize);

	virtual ~clMemMapper();
};

template<typename pType>
inline clMemMapper<pType>::clMemMapper(const cl_command_queue cl_queue, void** cl_malloc, const size_t cl_size, const cl_svm_mem_flags cl_flags) :
clMem( nullptr ), clOwner(false)
{
	clQueue = cl_queue;
	clMem = static_cast<pType*>(*cl_malloc);

	if (cl_size > 0) {
		cl_context ctx = NULL;

		::clGetCommandQueueInfo(clQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &ctx, NULL);
		cl_int status = 0;

		clMem = static_cast<pType*>(clSVMAlloc(ctx, cl_flags, cl_size * sizeof(pType), 0));
		*cl_malloc = clMem;
	}

	::clRetainCommandQueue(clQueue);
}

template<typename pType>
inline clMemMapper<pType>::clMemMapper(const cl_command_queue cl_queue, void* cl_malloc, const size_t cl_size, const cl_svm_mem_flags cl_flags) :
clMem( nullptr ), clOwner(false) {
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

template<typename pType>
inline pType* clMemMapper<pType>::clMapMem(cl_bool clBlocking, const cl_map_flags clFlags, const size_t clOff, const size_t clSize, cl_int* clStatus) {
    clBlocking = CL_TRUE;
    cl_int _clStatus = ::clEnqueueSVMMap( clQueue, clBlocking, clFlags, clMem, clSize * sizeof( pType ), 0, NULL, NULL );
    if (clStatus != nullptr) *clStatus = _clStatus;
    return clMem;
}

template<typename pType>
inline void clMemMapper<pType>::clWriteMem(cl_bool clBlocking, const size_t clOff, const size_t clSize, const void* srcPtr) {
    clBlocking = CL_TRUE;
    cl_int clStatus = ::clEnqueueSVMMemcpy( clQueue, clBlocking, clMem, srcPtr, clSize * sizeof( pType ), 0, NULL, NULL );
}

template<typename pType>
inline void clMemMapper<pType>::clFillMem(const pType pattern, const size_t clOff, const size_t clSize) {
    cl_int clStatus = ::clEnqueueSVMMemFill(clQueue, clMem, &pattern, sizeof(pType), clSize * sizeof(pType), 0, NULL, NULL);
}

template<typename pType>
inline clMemMapper<pType>::~clMemMapper() {
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

#endif /* CLMEMMAPPER_H_ */
