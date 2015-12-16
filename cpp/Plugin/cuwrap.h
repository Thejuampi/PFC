#ifndef _CUWRAP_H_
#define _CUWRAP_H_

extern "C" {

	typedef enum MemCopyDirection{
		C2G,
		G2C
	} MemCopyDirection;

	int si_cuda_malloc(void** ptr, unsigned int size);
	int si_cuda_free(void* ptr);
	int si_cuda_memcpy(void* dst, const void* src, unsigned int count, MemCopyDirection direction);

	int si_cuda_getFastestDeviceID();
	int si_cuda_getDeviceCount(int* count);
	int si_cuda_getDevice(int* device);
	int si_cuda_setDevice(int device);
};



#endif
