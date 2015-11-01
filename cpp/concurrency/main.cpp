#define __CL_ENABLE_EXCEPTIONS

#include "CL/cl.hpp"
#include <iostream>
//#include <thread>
//#include <mutex>
#include <algorithm>
#include "SDKUtil.hpp"
#include "SDKFile.hpp"
#include <string>
#include <set>
#include "oclutils.h"


using namespace std;

inline void handleStatus(cl_int status) {
    if(status != CL_SUCCESS) {
        cout << "status = " << OCLUtils::getOpenCLErrorCodeStr(status) << " = " << status << endl
             << "exiting" << endl;
        exit(-1);
    }
}

int main(int narg, char* args[])
{
    set<string> argumentos;
    if(narg > 0) {
        for(int c = 0; c < narg; ++c) {
            argumentos.insert(string(args[c]));
        }
    }

    cl_int status;              //utilizado para verificacion de errores
    cl_uint numPlatforms = 0;   // initialización de variable donde la API cargara el numero (cantidad) de plataformas.

    status = clGetPlatformIDs(0, NULL, &numPlatforms); //Obtengo la cantidad de plataformas (1 en este caso);
    handleStatus(status);
    cl_platform_id *platforms = NULL;

    platforms = (cl_platform_id*) malloc(numPlatforms*sizeof(cl_platform_id));

    status = clGetPlatformIDs(numPlatforms, platforms, NULL);
    handleStatus(status);

    cl_uint numDevices = 0;
    status = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
    handleStatus(status);

    cl_device_id *devices;
    devices = (cl_device_id*)malloc(numDevices*sizeof(cl_device_id));

    status = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);
    handleStatus(status);

    //el contexto engloba los dipositivos
    cl_context context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status );
    handleStatus(status);

    //en teoria hay que crear una cmdQueue por cada dispositivo
    cl_command_queue cmdQueue = clCreateCommandQueueWithProperties(context, devices[0], 0, &status);
    handleStatus(status);

    size_t datasize = 200*sizeof(cl_int);

    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY, datasize, NULL, &status);
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY, datasize, NULL, &status);

    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, datasize, NULL, &status);

    int A[200];
    int B[200];
    int C[200] = {0};

   memset(A, 5, 200*sizeof(int));
   memset(B, 4, 200*sizeof(int));

    appsdk::SDKFile file;
    if( !file.open("vecadd.cl") ) {
        cout << "Error al abrir vecadd.cl" << endl
             << "Saliendo";
        exit(-1);
    }
    const char * programSource[] = { file.source().c_str() };

    status = clEnqueueWriteBuffer(cmdQueue, bufA, CL_TRUE, 0, datasize, A, 0, NULL, NULL);
    handleStatus(status);

    status = clEnqueueWriteBuffer(cmdQueue, bufB, CL_TRUE, 0, datasize, B, 0, NULL, NULL);
    handleStatus(status);

    cl_program program = clCreateProgramWithSource(context, 1, programSource, NULL, &status);
    status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);
    if(status == CL_BUILD_PROGRAM_FAILURE) {
        size_t len;
        char *buffer = NULL;
        buffer = (char*) calloc(2048,sizeof(char));
        clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 2048*sizeof(char), buffer, &len );
        cout << buffer << endl;
        handleStatus(status);
    }

    cl_kernel kernel = clCreateKernel(program, "vecadd", &status);
    handleStatus(status);

    status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
    status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB);
    status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufC);
    handleStatus(status);

    size_t indexSpaceSize[1], workGroupSize[1];
    indexSpaceSize[0] = datasize/sizeof(cl_int);
    workGroupSize[0]  = 8;

    status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, indexSpaceSize, workGroupSize, 0, NULL, NULL);
    handleStatus(status);

    status = clEnqueueReadBuffer(cmdQueue, bufC, CL_TRUE, 0, datasize, C, 0, NULL, NULL);
    handleStatus(status);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(cmdQueue);
    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseContext(context);

    std::ofstream fSalida("resultado.csv");
    for(int c = 0; c < 199; ++c) {
        fSalida << C[c] << ", ";
    }
    fSalida <<C[199]<<endl;

    free(A);
    free(B);
    free(C);

    return 0;
}
