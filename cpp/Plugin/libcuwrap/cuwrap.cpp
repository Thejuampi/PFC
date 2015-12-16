
#include "cuwrap.h"
#include "cuda_runtime.h"
#include "stdlib.h"
#include "stdio.h"
#include <iostream>

int si_cuda_malloc(void** ptr, unsigned int size)
{
	return cudaMalloc(ptr,size);
}

int si_cuda_free(void* ptr)
{
	return cudaFree(ptr);
}

int si_cuda_memcpy(void* dst, const void* src, unsigned int count, MemCopyDirection direction)
{
	enum cudaMemcpyKind kind;

	switch (direction) {
	case C2G:
		kind = cudaMemcpyHostToDevice;
		break;
	case G2C:
		kind = cudaMemcpyDeviceToHost;
		break;
	default:
		return cudaErrorInvalidMemcpyDirection;
	};

	switch (cudaMemcpy(dst, src, count, kind)) {
	case cudaSuccess:
              return cudaSuccess;
		//	break ;
	case cudaErrorInvalidValue:
                return cudaErrorInvalidValue;
		//	break ;
	case cudaErrorInvalidDevicePointer:
                return cudaErrorInvalidDevicePointer;
		//	break ;
	case cudaErrorInvalidMemcpyDirection:
                return cudaErrorInvalidMemcpyDirection;
		//	break ;
	default:
            std::cout << cudaGetErrorString(cudaSuccess) << std::endl;
		return cudaErrorUnknown;
		//	break ;
	};
}

int si_cuda_getFastestDeviceID()
{
    int best_device = 0;
    int best_major = 0;
    int best_mpc = 0;

    int deviceCount;
        cudaGetDeviceCount(&deviceCount);
    if (deviceCount == 0)
    {
      std::cout << "Speedit crital error: no CUDA devices found!\nAborting...\n";
      exit (1);
    }

    for (int device = 0; device < deviceCount; ++device)
    {
      cudaDeviceProp deviceProp;
      cudaGetDeviceProperties(&deviceProp, device);
      if (device == 0)
      {
        if (deviceProp.major == 9999 && deviceProp.minor == 9999)
        {
          std::cout << "Speedit crital error: no CUDA devices found!\nAborting...\n";
          exit(1);
        }
      }

      if (deviceProp.major < 2)
        continue;
      if (deviceProp.major > best_major)
      {
        best_major  = deviceProp.major;
        best_mpc    = deviceProp.multiProcessorCount;
        best_device = device;
      }
      else if (deviceProp.major == best_major && deviceProp.multiProcessorCount > best_mpc)
      {
        best_mpc = deviceProp.multiProcessorCount;
        best_device = device;
      }
    }
    if (best_major < 2)
    {
      std::cout << "Speedit crital error: no CUDA devices of compute capability >= 2.0 found!\nAborting...\n";
      exit(1);
    }
    return best_device ;
}

int si_cuda_getDeviceCount(int* count)
{
	return cudaGetDeviceCount(count);

}

int si_cuda_getDevice( int* device)
{
	return cudaGetDevice(device);
}

int si_cuda_setDevice( int device )
{
	return cudaSetDevice(device);
}



















