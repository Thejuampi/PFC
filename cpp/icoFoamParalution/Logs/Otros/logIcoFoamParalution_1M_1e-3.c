/*---------------------------------------------------------------------------*\
| =========                 |                                                 |
| \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |
|  \\    /   O peration     | Version:  2.3.1                                 |
|   \\  /    A nd           | Web:      www.OpenFOAM.org                      |
|    \\/     M anipulation  |                                                 |
\*---------------------------------------------------------------------------*/
Build  : 2.3.1-bcfaaa7b8660
Exec   : ./icoFoamParalution
Date   : Oct 07 2015
Time   : 16:38:23
Host   : "juan-ubuntu"
PID    : 8742
Case   : /home/juan/dev/paralution-1.0.0/src/examples/OpenFOAM/icoFoamParalution
nProcs : 1
sigFpe : Enabling floating point exception trapping (FOAM_SIGFPE).
fileModificationChecking : Monitoring run-time modified files using timeStampMaster
allowSystemOperations : Allowing user-supplied system call operations

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
Create time

Create mesh for time = 0

Reading transportProperties

Reading field p

Reading field U

Reading/calculating face flux field phi

This version of PARALUTION is released under GPL.
By downloading this package you fully agree with the GPL license.
Number of CPU cores: 8
Host thread affinity policy - thread mapping on every core
Number of OpenCL devices in the system: 2
PARALUTION ver B1.0.0
PARALUTION platform is initialized
Accelerator backend: OpenCL
OpenMP threads:8
Selected OpenCL platform: 0
Selected OpenCL device: 0
------------------------------------------------
Platform number: 0
Platform name: AMD Accelerated Parallel Processing
Device number: 0
Device name: Hawaii
Device type: GPU
totalGlobalMem: 3943 MByte
clockRate: 947
OpenCL version: OpenCL 2.0 AMD-APP (1729.3)
------------------------------------------------
------------------------------------------------
Platform number: 0
Platform name: AMD Accelerated Parallel Processing
Device number: 1
Device name: Intel(R) Core(TM) i7-4770K CPU @ 3.50GHz
Device type: GPU
totalGlobalMem: 11963 MByte
clockRate: 4099
OpenCL version: OpenCL 1.2 AMD-APP (1729.3)
------------------------------------------------

Starting time loop

Time = 0.0001

Courant Number mean: 0 max: 0
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Ux, Initial residual = 1, Final residual = 9.42729e-06, No Iterations 57
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Uy, Initial residual = 0, Final residual = 0, No Iterations 0
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 1, Final residual = 0.000993268, No Iterations 2502
time step continuity errors : sum local = 1.98404e-08, global = 1.74579e-21, cumulative = 1.74579e-21
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.787611, Final residual = 0.000993261, No Iterations 2473
time step continuity errors : sum local = 2.51796e-08, global = 5.02611e-21, cumulative = 6.7719e-21
ExecutionTime = 17.58 s  ClockTime = 17 s

Time = 0.0002

Courant Number mean: 0.0227357 max: 0.939567
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Ux, Initial residual = 0.0637853, Final residual = 9.00717e-06, No Iterations 51
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Uy, Initial residual = 0.547642, Final residual = 4.90762e-06, No Iterations 59
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.970471, Final residual = 0.000998652, No Iterations 2444
time step continuity errors : sum local = 9.62408e-08, global = -1.18776e-20, cumulative = -5.10573e-21
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.92325, Final residual = 0.000950255, No Iterations 2441
time step continuity errors : sum local = 9.46391e-08, global = 6.1625e-23, cumulative = -5.0441e-21
ExecutionTime = 30.12 s  ClockTime = 29 s

Time = 0.0003

Courant Number mean: 0.0351999 max: 1.28015
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Ux, Initial residual = 0.062404, Final residual = 8.36317e-06, No Iterations 44
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Uy, Initial residual = 0.57015, Final residual = 9.77171e-06, No Iterations 54
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.990382, Final residual = 0.000981668, No Iterations 2357
time step continuity errors : sum local = 2.69424e-07, global = 5.92654e-21, cumulative = 8.82431e-22
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.963837, Final residual = 0.000967084, No Iterations 2351
time step continuity errors : sum local = 2.6816e-07, global = -4.39773e-21, cumulative = -3.5153e-21
ExecutionTime = 42.28 s  ClockTime = 40 s

Time = 0.0004

Courant Number mean: 0.0417798 max: 1.19369
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Ux, Initial residual = 0.105133, Final residual = 8.56854e-06, No Iterations 48
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Uy, Initial residual = 0.635068, Final residual = 7.98566e-06, No Iterations 58
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.994683, Final residual = 0.000992569, No Iterations 1559
time step continuity errors : sum local = 7.40562e-07, global = 3.53821e-21, cumulative = 2.29133e-23
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.965435, Final residual = 0.000986966, No Iterations 1460
time step continuity errors : sum local = 7.26948e-07, global = -1.89673e-20, cumulative = -1.89444e-20
ExecutionTime = 50.94 s  ClockTime = 48 s

Time = 0.0005

Courant Number mean: 0.0537009 max: 3.52898
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Ux, Initial residual = 0.190712, Final residual = 9.87148e-06, No Iterations 54
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PBiCG(paralution_MultiColoredILU):  Solving for Uy, Initial residual = 0.796787, Final residual = 9.72646e-06, No Iterations 64
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.996128, Final residual = 0.000999497, No Iterations 2354
time step continuity errors : sum local = 1.71747e-06, global = 6.06375e-21, cumulative = -1.28806e-20
*** warning: LocalMatrix::Permute() is performed on the host
*** warning: LocalMatrix::ILU0Factorize() is performed on the host
paralution_PCG(paralution_MultiColoredILU):  Solving for p, Initial residual = 0.941097, Final residual = 0.000994814, No Iterations 2350
time step continuity errors : sum local = 1.77772e-06, global = -1.11547e-20, cumulative = -2.40353e-20
ExecutionTime = 65.17 s  ClockTime = 61 s

End

